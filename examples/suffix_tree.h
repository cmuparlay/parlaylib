#include "parlay/sequence.h"
#include "parlay/primitives.h"

#include "longest_common_prefix.h"
#include "cartesian_tree.h"
#include "suffix_array.h"

// **************************************************************
// Suffix Tree.
// Give a string (sequence of characters), returns the root of a suffix tree.
// Each node contains:
//    -- a sequence of children
//    -- the depth of the node in the tree in terms of characters
//    -- the start position of a suffix of the input that matches
//       the path in the tree to the node.
// Uses the algorithm from:
//   A Simple Parallel Cartesian Tree Algorithm and its Application
//   to Parallel Suffix Tree Construction.
//   Julian Shun and Guy Blelloch
//   ACM Transactions on Parallel Computing, 2014.
// **************************************************************
template <typename unit>
struct suffix_tree {
  uint depth;
  uint start;
  parlay::sequence<suffix_tree*> children;

  suffix_tree(uint depth, uint start) : depth(depth), start(start) {}
  static parlay::type_allocator<suffix_tree> node_pool;
  
  template <typename Str>
  static suffix_tree* build(const Str& S) {
    uint n = S.size();
    parlay::internal::timer t;

    // first generate suffix array, lcps, and cartesian tree on lcps
    auto SA = suffix_array(S);
    auto LCP = lcp(S, SA);
    auto Parents = cartesian_tree(LCP);
    uint root = *parlay::find_if(parlay::iota(n-1), [&] (long i) {
	return Parents[i] == i;});
    
    // The binary cartesian tree will contain connected subtrees with the
    // same LCP value.   This identifies for each node its root in the subtree.
    auto get_cluster_root = [&] (uint i) {
      while (i != Parents[i] && LCP[i] == LCP[Parents[i]])
	  i = Parents[i];
      return i;};
    
    // Create internal nodes of the suffix tree from the cartesian tree.
    // Only roots of subtrees are kept.
    parlay::sequence<suffix_tree*> nodes(2*n-1, nullptr);
    auto internal = parlay::map_maybe(parlay::iota(n-1), [&] (uint i) {
	uint parent = Parents[i];
	if (parent != i && LCP[parent] == LCP[i]) // not a root
	  return std::optional<std::pair<uint,uint>>();
	else { // is a root
	  nodes[i] = node_pool.allocate(LCP[i],SA[i]);
	  parent = get_cluster_root(parent);
	  return std::optional{std::pair{parent, i}};
	}});
    
    // Generate the leaf nodes of the suffix tree, one per entry in the suffix array
    auto leaves = parlay::delayed_tabulate(n, [&] (uint i) {
	// for each element of SA find larger LCP on either side
	// the root of its subtree will be the parent in the suffix tree
	uint parent = (i == 0) ? 0 : ((i==n-1) ? i-1 : (LCP[i-1] > LCP[i] ? i-1 : i));
	parent = get_cluster_root(parent);
	nodes[i + (n - 1)] = node_pool.allocate(n-SA[i], SA[i]);
	return std::pair{parent, i + (n - 1)}; });
    
    // generate a mapping from nodes to their children
    auto groups = parlay::group_by_key_ordered(parlay::append(internal, leaves));
    parlay::for_each(std::move(groups), [&] (auto& g) {
	auto [parent, children] = g;
	nodes[parent]->children = parlay::map(children, [&] (uint j) {
	    return nodes[j];});});
    
    // return the root
    return nodes[root];
  }
};
