#include <limits>
#include <algorithm>
#include "parlay/primitives.h"
#include "parlay/delayed.h"
#include "parlay/internal/get_time.h"

// **************************************************************
// KD-tree on a set of 3d rectangles
// Builds the kd-tree top-down using the surface area heuristic (SAH)
// for selecting the "best" planar, axis-alligned cut at each node.
// For any cut some rectangles will fall completely on one side some or
// the other, and some will straddle the cut.  The straddlers will
// have to go to both sides.  The (heuristic) cost of a cut of a
// bounding box into A and B is given by:
//      minimize:  S_A * n_A + S_B * n_B,
// where S_x is the surface area of the box for A and B, and n_x is
// the number of rectangles intersecting each box.
//
// The minimum can be found by sweeping along each axis adding
// rectangle as the line first hits it, and removing it when the line
// passes it.  For each such event, calculate the cost of the cut at
// that point---we know the number of objects on the left and
// right and can easily calculate the surface areas.
// Done in parallel using a scan.
// **************************************************************

namespace delayed = parlay::delayed;
using index_t = int;

// Constants for deciding when to stop recursion in building the KDTree
// quit early if |Left| + |Right| > max_expand * |Input|
// or CT + CL * cut_cost > |Input|/(2*InputSurfaceArea)
float max_expand = 1.6; 
float CT = 6.0;
float CL = 1.25;

// an event when entering a box (start) or exiting the box (end).
// Each event has the coordinate, the index of the box, and whether start or end
// The last two are encoded in one value by borrowing the low-order bit
struct event {
  float v;  index_t p;
  bool is_start() const {return !(p & 1);}
  bool is_end() const {return p & 1;}
  index_t get_index() const {return p >> 1;}
  event(float value, index_t index, bool is_end_) 
    : v(value), p((index << 1) + is_end_) {}
  event() {}
};

using point = std::array<float, 3>;
using range = std::array<float, 2>;
using Events = std::array<parlay::sequence<event>, 3>;
using Bounding_Box = std::array<range, 3>;
using Boxes = parlay::sequence<Bounding_Box>;
using Ranges = std::array<parlay::sequence<range>, 3>;

struct cut_info {
  float cost; float cut_off;
  index_t num_left; index_t num_right;};

struct tree_node {
  tree_node *left, *right;
  Bounding_Box box;
  int cut_dim;
  float cut_off;
  parlay::sequence<index_t> box_indices;
  index_t n;
  index_t num_leaves;
  
  bool is_leaf() {return left == nullptr;}

  tree_node(tree_node* L, tree_node* R, 
	   int cut_dim, float cut_off, Bounding_Box B) 
    : left(L), right(R), cut_dim(cut_dim), 
      cut_off(cut_off), box(B) {
    n = L->n + R->n;
    num_leaves = L->num_leaves + R->num_leaves;
  }

  tree_node(Events E, index_t _n, Bounding_Box B)
  : left(NULL), right(NULL), box(B) {
    // extract indices from events
    event* events = E[0].begin();
    box_indices = parlay::sequence<index_t>(_n/2);
    index_t k = 0;
    for (index_t i = 0; i < _n; i++) 
      if (events[i].is_start()) box_indices[k++] = events[i].get_index();
    n = _n/2;
    num_leaves = 1;
  }

  static parlay::type_allocator<tree_node> node_allocator;

  ~tree_node() {
    if (!is_leaf()) {
      if (left == nullptr || right == nullptr) abort();
      parlay::par_do_if(n > 1000,
			[&] () { node_allocator.retire(left);},
			[&] () { node_allocator.retire(right);}
			);};
  }
};

float box_surface_area(Bounding_Box B) {
  float r0 = B[0][1]-B[0][0];
  float r1 = B[1][1]-B[1][0];
  float r2 = B[2][1]-B[2][0];
  return 2*(r0*r1 + r1*r2 + r0*r2);
}

// parallel best cut
cut_info best_cut(parlay::sequence<event> const &E, range r, range r1, range r2) {
  index_t n = E.size();
  float flt_max = std::numeric_limits<float>::max();
  if (r[1] - r[0] == 0.0) return cut_info{flt_max, r[0], n, n};

  // area of two orthogonal faces
  float orthog_area = 2 * ((r1[1]-r1[0]) * (r2[1]-r2[0]));

  // length of the perimeter of the orthogonal faces
  float orthog_perimeter = 2 * ((r1[1]-r1[0]) + (r2[1]-r2[0]));
  
  // count number that end before i
  auto is_end = delayed::tabulate(n, [&] (index_t i) -> index_t {return E[i].is_end();});
  auto end_counts = delayed::scan_inclusive(is_end);
  
  // calculate cost of each possible split location, 
  // return tuple with cost, number of ends before the location, and the index
  using rtype = std::tuple<float,index_t,index_t>;
  auto cost_f = [&] (std::tuple<index_t,index_t> ni) -> rtype {
    auto [ num_ends, i] = ni;
    index_t num_ends_before = num_ends - E[i].is_end(); 
    index_t in_left = i - num_ends_before; // number of points intersecting left
    index_t in_right = n/2 - num_ends;   // number of points intersecting right
    float left_length = E[i].v - r[0];
    float left_surface_area = orthog_area + orthog_perimeter * left_length;
    float right_length = r[1] - E[i].v;
    float right_surface_area = orthog_area + orthog_perimeter * right_length;
    float cost = left_surface_area * in_left + right_surface_area * in_right;
    return rtype(cost, num_ends_before, i);};
  auto costs = delayed::map(delayed::zip(end_counts, parlay::iota<index_t>(n)), cost_f);

  // find minimum across all, returning the triple
  auto min_f = [&] (rtype a, rtype b) {return (std::get<0>(a) < std::get<0>(b)) ? a : b;};
  rtype identity(std::numeric_limits<float>::max(), 0, 0);
  auto [cost, num_ends_before, i] =
    delayed::reduce(costs, parlay::binary_op(min_f, identity));
 
  index_t ln = i - num_ends_before;
  index_t rn = n/2 - (num_ends_before + E[i].is_end());
  return cut_info{cost, E[i].v, ln, rn};
}

using events_pair = std::pair<parlay::sequence<event>, parlay::sequence<event>>;

events_pair split_events(const parlay::sequence<range> &box_ranges,
			 const parlay::sequence<event> &events,
			 float cut_off) {
  index_t n = events.size();
  auto lower = parlay::sequence<bool>::uninitialized(n);
  auto upper = parlay::sequence<bool>::uninitialized(n);
  parlay::parallel_for (0, n, [&] (index_t i) {
      index_t b = events[i].get_index();
      lower[i] = box_ranges[b][0] < cut_off;
      upper[i] = box_ranges[b][1] > cut_off;}, 1000);

  return events_pair(parlay::pack(events, lower), parlay::pack(events, upper));
}

// n is the number of events (i.e. twice the number of triangles)
tree_node* generate_node(Ranges &boxes,
			 Events events,
			 Bounding_Box B, 
			 size_t maxDepth) {
  index_t n = events[0].size();
  if (n <= 2 || maxDepth == 0) 
    return tree_node::node_allocator.allocate(std::move(events), n, B);
  
  // loop over dimensions and find the best cut across all of them
  cut_info cuts[3];
  parlay::parallel_for(0, 3, [&] (size_t d) {
      cuts[d] = best_cut(events[d], B[d], B[(d+1)%3], B[(d+2)%3]);
    }, 10000/n + 1);
  
  int cut_dim = 0;
  for (int d = 1; d < 3; d++) 
    if (cuts[d].cost < cuts[cut_dim].cost) cut_dim = d;

  float cut_off = cuts[cut_dim].cut_off;
  float area = box_surface_area(B);
  float bestCost = CT + CL * cuts[cut_dim].cost/area;
  float origCost = (float) (n/2);

  // quit recursion early if best cut is not very good
  if (bestCost >= origCost || 
      cuts[cut_dim].num_left + cuts[cut_dim].num_right > max_expand * n/2)
    return tree_node::node_allocator.allocate(std::move(events), n, B);

  // declare structures for recursive calls
  Bounding_Box BBL = B;
  BBL[cut_dim] = range{BBL[cut_dim][0], cut_off};
  std::array<parlay::sequence<event>,3> left_events;

  Bounding_Box BBR = B;
  BBR[cut_dim] = range{cut_off, BBR[cut_dim][1]};
  std::array<parlay::sequence<event>,3> right_events;

  // now split each event array to the two sides
  parlay::parallel_for (0, 3, [&] (size_t d) {
      std::tie(left_events[d], right_events[d]) = split_events(boxes[cut_dim], events[d], cut_off);
    }, 10000/n + 1);

  // free (i.e. clear) old events and make recursive calls
  for (int i=0; i < 3; i++) events[i] = parlay::sequence<event>();
  tree_node *L, *R;
  parlay::par_do([&] () {L = generate_node(boxes, std::move(left_events),
					   BBL, maxDepth-1);},
		 [&] () {R = generate_node(boxes, std::move(right_events),
					   BBR, maxDepth-1);});
  return tree_node::node_allocator.allocate(L, R, cut_dim, cut_off, B);
}

auto kdtree_from_boxes(Boxes& boxes) {
  // Loop over the dimensions creating an array of events for each
  // dimension, sorting each one, and extracting the bounding box
  // from the first and last elements in the sorted events in each dim.
  Events events;
  Ranges ranges;
  Bounding_Box boundingBox;
  index_t n = boxes.size();
  for (int d = 0; d < 3; d++) {
    events[d] = parlay::sequence<event>(2*n);
    ranges[d] = parlay::sequence<range>(n);
    parlay::parallel_for(0, n, [&] (size_t i) {
	events[d][2*i] = event(boxes[i][d][0], i, false);
	events[d][2*i+1] = event(boxes[i][d][1], i, true);
	ranges[d][i] = boxes[i][d];});
    parlay::sort_inplace(events[d], [] (event a, event b) {return a.v < b.v;});
    boundingBox[d] = range{events[d][0].v, events[d][2*n-1].v};
  }

  // build the tree
  size_t recursion_depth = parlay::log2_up(n)-1;
  return generate_node(ranges, std::move(events), boundingBox, recursion_depth);
}
