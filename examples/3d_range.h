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

#include "oct_tree.h"

// **************************************************************
// Given a set of points reports for each point the other points within
// a sphere of radius r.
// Uses z-trees. See oct_tree.h and:
// **************************************************************

using ranges = parlay::sequence<parlay::sequence<idx>>;

// **************************************************************
// Search a z-tree for points within radius r of a point starting
// at a leaf.
//    search(node, point, radius)
// After construction the result is left in in_range.
// **************************************************************
struct search {
  point p; // the point being searched
  double r;  // radius to search within
  parlay::sequence<idx> in_range; // the points found within range 

  // The main search routine.  T must be a leaf node.
  search(node* T, point p, double r) : p(p), r(r) {
    add_leaf(T);

    // Move up tree searching subtrees and updating in_range
    // Can stop if radius is within the box.
    while ((!within_epsilon_box(T, -r)) &&
           (T->parent != nullptr)) {
      node* parent = T->parent;
      interior* pI = static_cast<interior*>(parent);
      if (T == pI->right) range_search_down(pI->left);
      else range_search_down(pI->right);
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

  // add points in a leaf node to in_range
  void add_leaf(node* T) {
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++)
      if (TL->pts[i].id != p.id &&
	  distance_squared(TL->pts[i].pnt) < r * r)
	in_range.push_back(TL->pts[i].id); }
  
  // looks for points within range for p in subtree rooted at T.
  // Can return immediately if radius does not intersect the box.
  void range_search_down(node* T) {
    if (within_epsilon_box(T, r)) {
      if (T->is_leaf) add_leaf(T);
      else {
        interior* TI = static_cast<interior*>(T);
	range_search_down(TI->left);
	range_search_down(TI->right);
      }
    }
  }
};

// **************************************************************
// Find points in range r for each point.
// Go down to each leaf, and then search from leaf.
// **************************************************************
void process_points_recursive(node* T, ranges& in_range, double r) {
  if (T->is_leaf) {
    for (int i=0; i < T->size; i++) {
      leaf* TL = static_cast<leaf*>(T);
      point p = TL->pts[i];
      search s(T, p, r); // the main search
      in_range[p.id] = std::move(s.in_range);
    }
  } else {
    interior* TI = static_cast<interior*>(T);
    long n_left = TI->left->size;
    long n = T->size;
    parlay::par_do([&] () {process_points_recursive(TI->left, in_range, r);},
                   [&] () {process_points_recursive(TI->right, in_range, r);});
  }
}

auto in_range(const parlay::sequence<coords>& P, double r) {
  node* T = z_tree(P);
  ranges result(P.size());
  process_points_recursive(T, result, r);
  delete_tree(T);
  return result;
}
