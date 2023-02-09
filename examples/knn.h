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
// K-Nearest-Neighbor Graph (constant dimensions)
// Uses z-trees.
// Nearest neighbor search starts at each leaf.
// See oct_tree.h and:
//   Magdalen Dobson and Guy E. Blelloch.
//   Parallel Nearest Neighbors in Low Dimensions with Batch Updates.
//   ALENEX 2022
// **************************************************************

using knn_graph = parlay::sequence<parlay::sequence<idx>>;

// **************************************************************
// Search the tree for the k-nearest neighbors of a point
//    search(node, point, k)
// After construction the result is left in candidates.
// **************************************************************
struct search {
  using neighbor = std::pair<idx,double>; // point id and distance from p
  point p; // the point being searched
  int k;
  static constexpr double inf = std::numeric_limits<double>::max();

  // The k candidate nearest neighbors, sorted with furtherst first
  parlay::sequence<neighbor> candidates;

  // The main search routine.  T must be a leaf node.
  search(node* T, point p, int k)
      : p(p), k(k), candidates(parlay::sequence<neighbor>(k, neighbor{-1,inf})) {

    // Insert leaf points in candidate list
    leaf* TL = static_cast<leaf*>(T);
    for (int i = 0; i < TL->size; i++)
      if (TL->pts[i].id != p.id) update_nearest(TL->pts[i]);

    // Move up tree searching subtrees and updating candidates.
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
  node* T = z_tree(P);
  knn_graph r(P.size());
  process_points_recursive(T, r, k);
  delete_tree(T);
  return r;
}
