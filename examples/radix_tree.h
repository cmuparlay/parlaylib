#include "parlay/sequence.h"
#include "parlay/primitives.h"

#include "cartesian_tree.h"

// **************************************************************
// Radix Tree (also called radix trie, compressed trie or, for binary
// alphabets, PATRICIA tree.).
// It is a trie with nodes with a single child removed and their two
// adjacent edges joined (hence edges have multiple characters).
// For a sorted sequence of strings this code takes the longest common
// prefix (LCP) between adjacent strings and returns the tree.  It
// does not need the actual strings since the LCP has enough
// information to build the tree.  The leaves of the tree are indices
// of the sorted strings (more info below).
// It uses a cartesian tree algorithm, see:
//   A Simple Parallel Cartesian Tree Algorithm and its Application
//   to Parallel Suffix Tree Construction.
//   Julian Shun and Guy Blelloch
//   ACM Transactions on Parallel Computing, 2014.
// **************************************************************
template <typename index>
struct radix_tree {
  // The tree is represented as a sequence of the internal nodes (tree)
  // and the index of the root (root) in the sequence.
  // Each node contains:
  //   -- the depth of the node in the tree in terms of characters
  //   -- the index of an input string that reaches that node
  //   -- a sequence of indices of its children
  // Each child index i is either:
  //   -- odd, in which case i/2 is the index of another internal node
  //   -- even, in which case i/2 is the index of the input string
  //      that reaches the leaf.
  // There are exactly n leaves corresponding to the n strings
  // The characters on an edge (p,c) are S[c.string_idx][p.depth,...,c.depth)].
  struct node {
    index depth;
    index string_idx;
    parlay::sequence<index> children;
  };
  parlay::sequence<node> tree;
  index root;
  static bool is_leaf(index p) {
    return (p & 1) == 0;}
  index get_string(index p) const {
    return is_leaf(p) ? p/2 : tree[p/2].string_idx; }
  index get_root() const { return 2*root+1;}
  // these two only work on internal nodes
  parlay::sequence<index> get_children(index p) const {
    return tree[p/2].children;}
  index get_depth(index p) const { return tree[p/2].depth;}
  
  template <typename LCPs>
  radix_tree (const LCPs& lcps) {
    index n = lcps.size()+1;

    // first generate a cartesian tree on the lcps
    auto Parents = cartesian_tree(lcps);

    // The binary cartesian tree will contain connected subtrees with
    // the same LCP value.  The following identifies the roots of such
    // subtrees, and labels each with an index.
    auto root_flags = parlay::tabulate(n - 1, [&] (index i) {
	return (i == Parents[i] || lcps[i] != lcps[Parents[i]]);});
    auto root_locs = parlay::pack_index(root_flags);
    index num_roots = root_locs.size();
    parlay::sequence<index> root_ids(n-1);
    parlay::parallel_for(0, num_roots, [&] (index i) {root_ids[root_locs[i]] = i;});

    // find root of subtree for location i
    auto cluster_root = [&] (index i) {
      while (i != Parents[i] && lcps[i] == lcps[Parents[i]])
	  i = Parents[i];
      return i;};
    // overall root
    root = root_ids[*parlay::find_if(root_ids, [&] (long i) {
	return Parents[i] == i;})];

    // For each root of a subtree return a pair of the index of its
    // parent subtree root node, and its index*2 + 1.
    auto internal = parlay::delayed_tabulate(num_roots, [&] (index i) {
	if (i == root) return std::pair{num_roots,2*i+1};
	index j = root_locs[i];
	index parent = root_ids[cluster_root(Parents[j])];
	return std::pair{parent, 2 * i + 1};});

    // For each leaf (one per input string) return a pair
    // of the index of its parent subtree root node, and 2* its index.
    auto leaves = parlay::delayed_tabulate(n, [&] (index i) {
	// for each element of SA find larger LCP on either side
	// the root of its subtree will be its parent in the radix tree
	index parent = (i == 0) ? 0 : ((i==n-1) ? i-1 : (lcps[i-1] > lcps[i] ? i-1 : i));
	parent = root_ids[cluster_root(parent)];
	return std::pair{parent, 2*i}; });

    // generate a mapping from node indices to indices of their children
    auto groups = parlay::group_by_index(parlay::append(internal, leaves), num_roots+1);
    
    // Create nodes. 
    tree = parlay::tabulate(num_roots, [&] (index i) {
	index j = root_locs[i];
	return node{lcps[j], j, std::move(groups[i])};});
  }

  radix_tree() {}
};
