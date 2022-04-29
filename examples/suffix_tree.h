#include "parlay/sequence.h"
#include "parlay/primitives.h"

#include "longest_common_prefix.h"
#include "cartesian_tree.h"
#include "suffix_array.h"

// **************************************************************
// Suffix Tree.
// Give a string (sequence of characters), returns a suffix tree.
// Uses the algorithm from:
//   A Simple Parallel Cartesian Tree Algorithm and its Application
//   to Parallel Suffix Tree Construction.
//   Julian Shun and Guy Blelloch
//   ACM Transactions on Parallel Computing, 2014.
// **************************************************************
template <typename unit>
struct suffix_tree {
  // The tree is represented with a sequence of nodes (tree) and a root (root)
  // Each node contains:
  //   -- the depth of the node in the tree in terms of characters
  //   -- the start position of a suffix of the input that matches
  //      the path in the tree to the node.
  //   -- a sequence of indices of its children (empty if a leaf)
  // There are exactly n leaves corresponding to the n suffixes
  // Internal nodes go first, then leaves
  // The root is the index of the root node
  template <typename uint>
  struct node {
    uint depth;
    uint start;
    parlay::sequence<uint> children;
  };
  parlay::sequence<node<uint>> tree;
  uint root;

  template <typename Str>
  suffix_tree (const Str& S) {
    uint n = S.size();

    // first generate suffix array, lcps, and cartesian tree on lcps
    auto SA = suffix_array(S);
    auto LCP = lcp(S, SA);
    auto Parents = cartesian_tree(LCP);
    root = *parlay::find_if(parlay::iota(n-1), [&] (long i) {
	return Parents[i] == i;});
    
    // The binary cartesian tree will contain connected subtrees with the
    // same LCP value.   This identifies for each node its root in the subtree.
    auto cluster_root = [&] (uint i) {
      while (i != Parents[i] && LCP[i] == LCP[Parents[i]])
	  i = Parents[i];
      return i;};
    
    // For each root of a subtree return a pair of the index of its
    // parent subtree root node, and its index.
    auto internal = parlay::map_maybe(parlay::iota(n-1), [&] (uint i) {
	uint parent = Parents[i];
	if (parent != i && LCP[parent] == LCP[i]) // not a root
	  return std::optional<std::pair<uint,uint>>();
	else // is a root
	  return std::optional{std::pair{cluster_root(parent), i}};});;
	
    // For each leaf node (one per entry in suffix array) return a
    // pair of the index of its parent subtree root node, and its index.
    auto leaves = parlay::delayed_tabulate(n, [&] (uint i) {
	// for each element of SA find larger LCP on either side
	// the root of its subtree will be its parent in the suffix tree
	uint parent = (i == 0) ? 0 : ((i==n-1) ? i-1 : (LCP[i-1] > LCP[i] ? i-1 : i));
	return std::pair{cluster_root(parent), n + i - 1}; });
	
    // generate a mapping from node indices to indices of their children
    auto groups = parlay::group_by_index(parlay::append(internal, leaves), n-1);

    // Create nodes. Internal nodes at front (n-1 of them), then leaves (n of them).
    // Internal nodes that are not roots are unused.
    tree = parlay::tabulate(2*n - 1, [&] (uint i) {
	return ((i < n - 1) ?
		node<uint>{LCP[i], SA[i], std::move(groups[i])} :
		node<uint>{n-SA[i-n+1], SA[i-n+1], parlay::sequence<uint>()});});
  }

  suffix_tree() {}
};
