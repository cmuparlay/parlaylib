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
// Builds the z-tree variant of oct-trees.
//   Magdalen Dobson and Guy E. Blelloch.
//   Parallel Nearest Neighbors in Low Dimensions with Batch Updates.
//   ALENEX 2022
// Sorts based on Morton ordering and then builds compressed oct tree
// on top of it (i.e. no nodes with a single child).
// **************************************************************

// type definitions
constexpr int dims = 3; // works for any constant dimension
using idx = int; // index of point (int can handle up to 2^31 points)
using coord = int; // type of each coordinate
using coords = std::array<coord,dims>;
struct point {idx id; coords pnt;};
using points = parlay::sequence<point>;

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
node* build_recursive(slice P, int bit, long base_size) {
  long n = P.size();
  if (n == 0) abort();

  // if ran out of bits, or small then generate a leaf
  if (bit == 0 || n < base_size) {
    return leaf_allocator.allocate(parlay::to_sequence(P));
  } else {

    // binary search for the cut point on the given bit
    auto bits = parlay::delayed::map(P, [&] (const point& p) {
      return 1 == ((p.pnt[dims-bit%dims-1] >> bit/dims) & 1);});
    long pos = std::lower_bound(bits.begin(), bits.end(), 1)-bits.begin();

    // if all points are on one side, then move onto the next bit
    if (pos == 0 || pos == n) return build_recursive(P, bit - 1, base_size);

    // otherwise recurse on the two parts, also moving to next bit
    else {
      node *L, *R;
      parlay::par_do([&] () {L = build_recursive(P.cut(0, pos), bit - 1, base_size);},
                     [&] () {R = build_recursive(P.cut(pos, n), bit - 1, base_size);});
      return interior_allocator.allocate(L,R);
    }
  }
}

auto z_tree(const parlay::sequence<coords>& P, long base_size = node_size_cutoff) {
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
  return build_recursive(pts.cut(0, P.size()), nbits-1, base_size);
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
