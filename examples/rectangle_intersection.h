#include <algorithm>

#include "parlay/primitives.h"
#include "parlay/parallel.h"
#include "parlay/sequence.h"

#include "box_kdtree.h"

// **************************************************************
// Reports for a set of rectangles in 3d the other rectangle they intersect.
// Rectangles must be stored in a kd-tree, where every rectangle appears
// in every leaf of the tree for which it intersects.
// Can use the the surface area heuristic (SAH) to build the tree, but
// any method will work.
// See box_kdtree for format of tree.
// Does not report rectangles that intersect at a boundary.
// **************************************************************

using pair_seq = parlay::sequence<std::pair<index_t,index_t>>;

bool intersect(const Bounding_Box& a, const Bounding_Box& b) {
  for (int i=0; i < 3; i++) 
    if (a[i][1] <= b[i][0] || a[i][0] >= b[i][1])
      return false;
  return true;
}

void process_recursive(tree_node* T, const Boxes &rectangles, pair_seq* results) {
  if (T->is_leaf()) {
    // get all intersections within the leaf
    *results = parlay::flatten(parlay::tabulate(T->n, [&] (index_t i) {
	  index_t idx_a = T->box_indices[i];
	  auto x = parlay::tabulate(T->n - i - 1, [&] (long j) {
	      index_t idx_b = T->box_indices[i+j+1];
	      if (!intersect(rectangles[idx_a], rectangles[idx_b]))
	  	return std::pair{-1,-1};
	      else return std::pair{std::min(idx_a, idx_b), std::max(idx_a, idx_b)};}, 1000);
	  return filter(x, [] (auto p) {return p.first > -1;});}, 100));
  } else {
    // recurse to two subtrees
    parlay::par_do([&] {process_recursive(T->left, rectangles, results);},
		   [&] {process_recursive(T->right, rectangles,
					  results + T->left->num_leaves);});
  }
}
			
auto rectangle_intersection(Boxes &rectangles) {
  tree_node* R = kdtree_from_boxes(rectangles);
  parlay::sequence<pair_seq> results(R->num_leaves);
  process_recursive(R, rectangles, results.data());
  tree_node::node_allocator.retire(R);
  auto pairs = parlay::flatten(std::move(results));
  return parlay::remove_duplicates(std::move(pairs));
}
 
