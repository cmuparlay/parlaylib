#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <utility>

#include "parlay/alloc.h"
#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/delayed.h"
#include "parlay/sequence.h"    // for sequence, sequence<>::value_type, to_...
#include "parlay/slice.h"       // for slice, make_slice
#include "parlay/utilities.h"   // for par_do_if

// **************************************************************
// K-Nearest-Neighbor Graph (constant dimensions)
// For each point find its k closest points and return as a sparse graph
// Uses z-trees, which are based on sorting in the morton ordering
// and building an oct-tree like tree over it.   See:
//   Magdalen Dobson and Guy E. Blelloch.
//   Parallel Nearest Neighbors in Low Dimensions with Batch Updates.
//   ALENEX 2022
// Nearest neighbor search starts at each leaf.
// **************************************************************

// type definitions
constexpr int dims = 3;
using idx = int; // index of point (int can handle up to 2^31 points)
using coord = int; // type of each coordinate
using coords = std::array<coord,dims>;
using box = std::pair<coords,coords>;
struct point {idx id; coords pnt;};
using points = parlay::sequence<point>;
using knn_graph = parlay::sequence<parlay::sequence<idx>>;

constexpr int node_size_cutoff = 25;

// **************************************************************
// bounding box
// **************************************************************
auto minv = [] (coords a, coords b) {
  coords r;
  for (int i=0; i < dims; i++) r[i] = std::min(a[i], b[i]);
  return r;};

auto maxv = [] (coords a, coords b) {
  coords r;
  for (int i=0; i < dims; i++) r[i] = std::max(a[i], b[i]);
  return r;};

coords center(box b) {
  coords r;
  for (int i=0; i < dims; i++) r[i] = (b.first[i] + b.second[i])/2;
  return r;}

box bound_box(const parlay::sequence<point>& P) {
  auto pts = parlay::map(P, [] (point p) {return p.pnt;});
  return box{parlay::reduce(pts, parlay::binary_op(minv, coords())),
             parlay::reduce(pts, parlay::binary_op(maxv, coords()))};
}

box bound_box(const box& b1, const box& b2) {
  return box{min(b1.first, b2.first),
             max(b1.second, b2.second)};
}

// **************************************************************
// Tree structure, leafs and internal extend the base node class
// **************************************************************
struct node { bool is_leaf; idx size; box bounds; node* parent; };

struct leaf : node {
  points pts;
  leaf(points pts)
      : node{true, static_cast<idx>(pts.size()), bound_box(pts), nullptr},
        pts(pts) {}
};

struct internal : node {
  node* left;
  node* right;
  internal(node* left, node* right)
      : node{false, left->size+right->size,
             bound_box(left->bounds,right->bounds),nullptr},
        left(left), right(right) {
    left->parent = this; right->parent = this; }
};

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<internal> internal_allocator;

// **************************************************************
// Build the tree
// **************************************************************
template <typename slice>
node* build_recursive(slice P, int bit) {
  size_t n = P.size();
  if (n == 0) abort();

  // if ran out of bits, or small then generate a leaf
  if (bit == 0 || n < node_size_cutoff) {
    return leaf_allocator.allocate(parlay::to_sequence(P));
  } else {

    // binary search for the cut point on the given bit
    auto bits = parlay::delayed::map(P, [&] (const point& p) {
      return 1 == ((p.pnt[bit%dims] >> bit/dims) & 1);});
    size_t pos = std::lower_bound(bits.begin(), bits.end(), 1)-bits.begin();

    // if all points are on one side, then move onto the next bit
    if (pos == 0 || pos == n) return build_recursive(P, bit - 1);

      // otherwise recurse on the two parts, also moving to next bit
    else {
      node *L, *R;
      parlay::par_do_if(n > 1000,
                        [&] () {L = build_recursive(P.cut(0, pos), bit - 1);},
                        [&] () {R = build_recursive(P.cut(pos, n), bit - 1);});
      return internal_allocator.allocate(L,R);
    }
  }
}

auto build_tree(const parlay::sequence<coords>& P) {
  // compares the interleaved bits of points p and q without explicitly
  // interleaving them
  auto less = [] (const point& p, const point& q) {
    int j, k;
    coord y, x = 0;
    auto less_msb = [] (coord x, coord y) { return x < y && x < (x^y);};
    for (j = k = 0; k < dims; k++ )
      if (less_msb(x, y = p.pnt[k] ^ q.pnt[k])) {
        j = k; x = y;}
    return p.pnt[j] < q.pnt[j];
  };
  points pts = parlay::tabulate(P.size(), [&] (long i) {
    return point{(int) i, P[i]};});
  pts = parlay::sort(pts, less);
  int nbits = dims*sizeof(coord)*8;
  return  build_recursive(parlay::make_slice(pts), nbits-1);
}

void delete_tree(node* T) { // delete tree in parallel
  if (T->is_leaf) leaf_allocator.retire(static_cast<leaf*>(T));
  else {
    internal* TI = static_cast<internal*>(T);
    parlay::par_do_if(T->size > 1000, [&] {delete_tree(TI->left);},
                      [&] {delete_tree(TI->right);});
    internal_allocator.retire(TI);
  }
}

// **************************************************************
// Search the tree
// **************************************************************
struct search {
  using neighbor = std::pair<idx,double>;
  point p;
  int k;
  static constexpr double inf = std::numeric_limits<double>::max();

  // candidate nearest neighbors, sorted with furtherst first
  parlay::sequence<neighbor> candidates;

  // the main search routine, on construction
  // the result is left in candidates
  search(node* T, point p, int k)
      : p(p), k(k), candidates(parlay::sequence<neighbor>(k, neighbor{-1,inf})) {

    // insert leaf nodes in candidate list
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++)
      if (TL->pts[i].id != p.id) update_nearest(TL->pts[i]);

    // move up tree searching subtrees updating candidates
    // can stop if radius to current k-th nearest point is within the box
    while ((!within_epsilon_box(T, -sqrt(candidates[0].second))) &&
           (T->parent != nullptr)) {
      node* parent = T->parent;
      internal* pI = static_cast<internal*>(parent);
      if (T == pI->right) k_nearest_down(pI->left);
      else k_nearest_down(pI->right);
      T = parent;
    }
  }

  double distance_sq(coords q) {
    double r = 0.0;
    for (int i = 0; i < dims; i++) {
      double diff = (q[i] - p.pnt[i]);
      r += diff*diff; }
    return r; }

  bool within_epsilon_box(node* T, double epsilon) {
    box bounds;
    bool result = true;
    for (int i = 0; i < dims; i++)
      result = (result &&
                (bounds.first[i] - epsilon < p.pnt[i]) &&
                (bounds.second[i] + epsilon > p.pnt[i]));
    return result;  }

  // if p is closer than candidates[0] then swap it in
  void update_nearest(point q) {
    double d = distance_sq(q.pnt);
    if (d < candidates[0].second) {
      candidates[0] = neighbor{q.id, distance_sq(q.pnt)};
      for (int i = 1;
           i < k && candidates[i-1].second < candidates[i].second; i++)
        swap(candidates[i-1], candidates[i]);
    }
  }

  // looks for nearest neighbors for pt in subtree rooted at T.
  // Can return immediately if radius to current k-th nearest point
  // does not intersect the box.
  void k_nearest_down(node* T) {
    if (within_epsilon_box(T, sqrt(candidates[0].second))) {
      if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);
        for (int i = 0; i < TL->size; i++)
          if (TL->pts[i].id != p.id) update_nearest(TL->pts[i]);
      } else {
        internal* TI = static_cast<internal*>(T);
        if (distance_sq(center(TI->left->bounds)) < distance_sq(center(TI->right->bounds))) {
          k_nearest_down(TI->left);
          k_nearest_down(TI->right);
        } else {
          k_nearest_down(TI->right);
          k_nearest_down(TI->left);
        }
      }
    }
  }
};

// **************************************************************
// Find nearest neighbor for each point, starting at the leaves
// **************************************************************
void process_points_recursive(node* T, knn_graph& knn, int k) {
  if (T->is_leaf)
    for (int i=0; i < T->size; i++) {
      leaf* TL = static_cast<leaf*>(T);
      search s(T, TL->pts[i], k); // the main search
      knn[TL->pts[i].id] = parlay::map(s.candidates, [] (auto x) {
        return x.first;});
    }
  else {
    internal* TI = static_cast<internal*>(T);
    size_t n_left = TI->left->size;
    size_t n = T->size;
    parlay::par_do_if(n > 10,
                      [&] () {process_points_recursive(TI->left, knn, k);},
                      [&] () {process_points_recursive(TI->right, knn, k);});
  }
}

auto build_knn_graph(const parlay::sequence<coords>& P, int k) {
  node* T = build_tree(P);
  knn_graph r(P.size());
  process_points_recursive(T, r, k);
  delete_tree(T);
  return r;
}

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: knn <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    int k = 5;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_int_distribution<coord> dis(0,1000000000);

    // generate n random points in a unit square
    auto points = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      coords pnt;
      for (coord& c : pnt) c = dis(r);
      return pnt;
    });
    knn_graph r;
    for (int i=0; i < 3; i++) {
      r = build_knn_graph(points, k);
    }
    std::cout << k << " nearest neighbor graph for " << r.size()
              << " points" << std::endl;
  }
}

