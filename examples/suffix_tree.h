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
template <typename index>
struct suffix_tree {
  // The tree is represented as a sequence of the internal nodes (tree)
  // and the index of the root (root) in the sequence.
  // Each node contains:
  //   -- the depth of the node in the tree in terms of characters
  //   -- the start position of a suffix of the input that matches
  //      the path in the tree to the node.
  //   -- a sequence of indices of its children
  // Each child index i is either:
  //   -- odd, in which case i/2 is the index of another internal node
  //   -- even, in which case i/2 is an index into the input string
  //      corresponding to a leaf (i.e. following a path from the root).
  // There are exactly n leaves corresponding to the n suffixes
  struct node {
    index depth;
    index start;
    parlay::sequence<index> children;
  };
  parlay::sequence<node> tree;
  index root;

  template <typename Str>
  suffix_tree (const Str& S) {
    index n = S.size();

    // first generate suffix array, lcps, and cartesian tree on lcps
    auto SA = suffix_array(S);
    auto LCP = lcp(S, SA);
    auto Parents = cartesian_tree(LCP);
    root = *parlay::find_if(parlay::iota(n-1), [&] (long i) {
	return Parents[i] == i;});
    
    // The binary cartesian tree will contain connected subtrees with
    // the same LCP value.  The following identifies the roots of such
    // subtrees, and labels each with an index.
    auto root_flags = parlay::tabulate(n - 1, [&] (index i) {
	return (i == Parents[i] || LCP[i] != LCP[Parents[i]]);});
    auto root_locs = parlay::pack_index(root_flags);
    index num_roots = root_locs.size();
    parlay::sequence<index> root_ids(n-1);
    parlay::parallel_for(0, num_roots-1, [&] (index i) {root_ids[root_locs[i]] = i;});

    // find root of subtree for location i
    auto cluster_root = [&] (index i) {
      while (i != Parents[i] && LCP[i] == LCP[Parents[i]])
	  i = Parents[i];
      return i;};

    // For each root of a subtree return a pair of the index of its
    // parent subtree root node, and its index*2 + 1.
    auto internal = parlay::delayed_tabulate(num_roots, [&] (index i) {
	index j = root_locs[i];
	index parent = root_ids[cluster_root(Parents[j])];
	return std::pair{parent, 2 * i + 1};});;
	
    // For each leaf (one per entry in the suffix array) return a pair
    // of the index of its parent subtree root node, and 2* its SA entry.
    auto leaves = parlay::delayed_tabulate(n, [&] (index i) {
	// for each element of SA find larger LCP on either side
	// the root of its subtree will be its parent in the suffix tree
	index parent = (i == 0) ? 0 : ((i==n-1) ? i-1 : (LCP[i-1] > LCP[i] ? i-1 : i));
	parent = root_ids[cluster_root(parent)];
	return std::pair{parent, 2*SA[i]}; });
	
    // generate a mapping from node indices to indices of their children
    auto groups = parlay::group_by_index(parlay::append(internal, leaves), num_roots);

    // Create nodes. 
    tree = parlay::tabulate(num_roots, [&] (index i) {
	index j = root_locs[i];
	return node{LCP[j], SA[j], std::move(groups[i])};});
  }

  suffix_tree() {}
};
