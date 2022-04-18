#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "parlay/alloc.h"
#include "parlay/primitives.h"
#include "parlay/delayed.h"
#include "parlay/sequence.h"
#include "parlay/utilities.h"

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
constexpr int dims = 3; // works for any constant dimension
using idx = int; // index of point (int can handle up to 2^31 points)
using coord = int; // type of each coordinate
using coords = std::array<coord,dims>;
struct point {idx id; coords pnt;};
using points = parlay::sequence<point>;
using knn_graph = parlay::sequence<parlay::sequence<idx>>;

// max leaf size of tree
constexpr int node_size_cutoff = 20;

// **************************************************************
// bounding box (min value on each dimension, and max on each)
// **************************************************************
using box = std::pair<coords,coords>;

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
  auto x = box{parlay::reduce(pts, parlay::binary_op(minv, coords())),
               parlay::reduce(pts, parlay::binary_op(maxv, coords()))};
  return x;
}

box bound_box(const box& b1, const box& b2) {
  return box{minv(b1.first, b2.first),
             maxv(b1.second, b2.second)};
}

// **************************************************************
// Tree structure, leafs and interior extend the base node class
// **************************************************************
struct node { bool is_leaf; idx size; box bounds; node* parent; };

struct leaf : node {
  points pts;
  leaf(points pts)
      : node{true, static_cast<idx>(pts.size()), bound_box(pts), nullptr},
        pts(pts) {}
};

struct interior : node {
  node* left;
  node* right;
  interior(node* left, node* right)
      : node{false, left->size+right->size,
             bound_box(left->bounds,right->bounds),nullptr},
        left(left), right(right) {
    left->parent = this; right->parent = this; }
};

parlay::type_allocator<leaf> leaf_allocator;
parlay::type_allocator<interior> interior_allocator;

// **************************************************************
// Build the tree
// **************************************************************
template <typename slice>
node* build_recursive(slice P, int bit) {
  long n = P.size();
  if (n == 0) abort();

  // if ran out of bits, or small then generate a leaf
  if (bit == 0 || n < node_size_cutoff) {
    return leaf_allocator.allocate(parlay::to_sequence(P));
  } else {

    // binary search for the cut point on the given bit
    auto bits = parlay::delayed::map(P, [&] (const point& p) {
      return 1 == ((p.pnt[dims-bit%dims-1] >> bit/dims) & 1);});
    long pos = std::lower_bound(bits.begin(), bits.end(), 1)-bits.begin();

    // if all points are on one side, then move onto the next bit
    if (pos == 0 || pos == n) return build_recursive(P, bit - 1);

      // otherwise recurse on the two parts, also moving to next bit
    else {
      node *L, *R;
      parlay::par_do([&] () {L = build_recursive(P.cut(0, pos), bit - 1);},
                     [&] () {R = build_recursive(P.cut(pos, n), bit - 1);});
      return interior_allocator.allocate(L,R);
    }
  }
}

auto build_tree(const parlay::sequence<coords>& P) {
  // compares the interleaved bits of points p and q without explicitly
  // interleaving them.  From Timothy Chan.
  auto less = [] (const point& p, const point& q) {
    int j, k;
    coord y, x = 0;
    auto less_msb = [] (coord x, coord y) { return x < y && x < (x^y);};
    for (j = k = 0; k < dims; k++ )
      if (less_msb(x, y = p.pnt[k] ^ q.pnt[k])) {
        j = k; x = y;}
    return p.pnt[j] < q.pnt[j];
  };

  // tag points with identifiers
  points pts = parlay::tabulate(P.size(), [&] (idx i) {
    return point{i, P[i]};});

  // sort by morton ordering
  pts = parlay::sort(pts, less);
  int nbits = dims*sizeof(coord)*8;

  // build tree on top of morton ordering
  return build_recursive(pts.cut(0, P.size()), nbits-1);
}

void delete_tree(node* T) { // delete tree in parallel
  if (T->is_leaf) leaf_allocator.retire(static_cast<leaf*>(T));
  else {
    interior* TI = static_cast<interior*>(T);
    parlay::par_do_if(T->size > 1000,
                      [&] {delete_tree(TI->left);},
                      [&] {delete_tree(TI->right);});
    interior_allocator.retire(TI);
  }
}

// **************************************************************
// Search the tree for the k-nearest neighbors of a point
//    search(node, point, k)
// After construction the result is left in candidates.
// **************************************************************
struct search {
  using neighbor = std::pair<idx,double>;
  point p;
  int k;
  static constexpr double inf = std::numeric_limits<double>::max();

  // Candidate nearest neighbors, sorted with furtherst first
  parlay::sequence<neighbor> candidates;

  // The main search routine.
  search(node* T, point p, int k)
      : p(p), k(k), candidates(parlay::sequence<neighbor>(k, neighbor{-1,inf})) {

    // Insert leaf nodes in candidate list
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++)
      if (TL->pts[i].id != p.id) update_nearest(TL->pts[i]);

    // Move up tree searching subtrees updating candidates.
    // Can stop if radius to current k-th nearest point is within the box.
    while ((!within_epsilon_box(T, -sqrt(candidates[0].second))) &&
           (T->parent != nullptr)) {
      node* parent = T->parent;
      interior* pI = static_cast<interior*>(parent);
      if (T == pI->right) k_nearest_down(pI->left);
      else k_nearest_down(pI->right);
      T = parent;
    }
  }

  // distance from p to q squared
  double distance_squared(coords q) {
    double r = 0.0;
    for (int i = 0; i < dims; i++) {
      double diff = (q[i] - p.pnt[i]);
      r += diff*diff; }
    return r; }

  // Does the box for T intersect an epsilon ball around p?
  // Positive epsilon to test if strictly outside, and negative
  // to test if strictly inside.  Can return false positive.
  bool within_epsilon_box(node* T, double epsilon) {
    bool result = true;
    int i;
    for (i = 0; i < dims; i++) {
      result = (result &&
                (T->bounds.first[i] - epsilon < p.pnt[i]) &&
                (T->bounds.second[i] + epsilon > p.pnt[i]));
    }
    return result; }

  // if q is closer to p than candidates[0] then swap it in
  void update_nearest(point q) {
    double d = distance_squared(q.pnt);
    if (d < candidates[0].second) {
      candidates[0] = neighbor{q.id, distance_squared(q.pnt)};
      for (int i = 1;
           i < k && candidates[i-1].second < candidates[i].second; i++)
        swap(candidates[i-1], candidates[i]);
    }
  }

  // looks for nearest neighbors for p in subtree rooted at T.
  // Can return immediately if radius to current k-th nearest point
  // does not intersect the box.
  void k_nearest_down(node* T) {
    if (within_epsilon_box(T, sqrt(candidates[0].second))) {
      if (T->is_leaf) {
        leaf* TL = static_cast<leaf*>(T);
        for (int i = 0; i < TL->size; i++)
          if (TL->pts[i].id != p.id) update_nearest(TL->pts[i]);
      } else {
        interior* TI = static_cast<interior*>(T);
        if (distance_squared(center(TI->left->bounds)) <
            distance_squared(center(TI->right->bounds))) {
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
// Find nearest neighbor for each point.
// Go down to each leaf, and then search from leaf.
// **************************************************************
void process_points_recursive(node* T, knn_graph& knn, int k) {
  if (T->is_leaf) {
    for (int i=0; i < T->size; i++) {
      leaf* TL = static_cast<leaf*>(T);
      point p = TL->pts[i];
      search s(T, p, k); // the main search
      // write result (k nearest neighbors for p) in correct location
      knn[p.id] = parlay::map(s.candidates, [] (auto x) {
        return x.first;}, k);
    }
  } else {
    interior* TI = static_cast<interior*>(T);
    long n_left = TI->left->size;
    long n = T->size;
    parlay::par_do([&] () {process_points_recursive(TI->left, knn, k);},
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
