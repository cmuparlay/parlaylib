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
//    -- the depth of the node in the tree in terms of charactes
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
    long n = S.size();

    // first generate suffix array, lcps, and cartesian tree on lcps
    auto SA = suffix_array(S);
    auto LCP = lcp(S, SA);
    auto Parents = cartesian_tree(LCP);

    // Create internal nodes of the suffix tree from the cartesian tree.
    // Connected subtrees in the cartesian tree with the same depth need
    // to be collapsed to a single node (only the root of a subtree is allocated).
    auto internal = parlay::tabulate(n-1, [&] (uint i) {
	uint parent = Parents[i];
	while (parent != Parents[parent] && LCP[parent] == LCP[Parents[parent]])
	  parent = Parents[parent];
	if (Parents[i] != i && LCP[Parents[i]] == LCP[i])
	  return std::pair{parent, (suffix_tree*) nullptr};
	else return std::make_pair(parent, node_pool.allocate(LCP[i],SA[i]));
      });

    // Filter out nodes that are not a root of a subtree
    auto filtered = parlay::map_maybe(internal, [&] (auto x) {
	auto [parent, node] = x;
	if (node == nullptr && internal[parent].second == nullptr) abort();
	return ((node == nullptr || internal[parent].second == node) ? std::nullopt :
		std::optional{std::pair{internal[parent].second, node}});
      });

    // Generate the leaf nodes of the tree, one per entry in the suffix array
    auto leaves = parlay::delayed_tabulate(n, [&] (uint i) {
	// for each element of SA find larger LCP on either side
	// the root of it will be the parent in the suffix tree
	uint parent = (i == 0) ? 0 : ((i==n-1) ? i-1 : (LCP[i-1] > LCP[i] ? i-1 : i));
	auto new_node = node_pool.allocate(n-SA[i], SA[i]);
	if (internal[parent].second == nullptr)
	  parent = internal[parent].first;
	return std::make_pair(internal[parent].second, new_node);});

    // generate a mapping from nodes to their children
    auto x = parlay::group_by_key_ordered(parlay::append(filtered, leaves));
    parlay::for_each(x, [&] (auto& y) {
	y.first->children = std::move(y.second);});

    // find the root
    return parlay::find_if(x, [&] (auto& y) {return y.first->depth == 0;})->first;
  }
};
