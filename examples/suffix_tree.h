#include "suffix_array.h"
#include "longest_common_prefix.h"
#include "radix_tree.h"
#include "parlay/io.h"

// **************************************************************
// Suffix Tree.
// Give a string (sequence of characters), returns a suffix tree.
// Uses the algorithm from:
//   A Simple Parallel Cartesian Tree Algorithm and its Application
//   to Parallel Suffix Tree Construction.
//   Julian Shun and Guy Blelloch
//   ACM Transactions on Parallel Computing, 2014.
// The format of the output tree is given in "radix_tree.h"
// **************************************************************
template <typename index, typename Str>
radix_tree<index> suffix_tree(const Str& S) {
  auto SA = suffix_array(S);
  auto LCP = lcp(S, SA);
  radix_tree<index> result(LCP);

  // replace indices with locations in S
  parlay::for_each(result.tree, [&] (auto& node) {
      node.string_idx = SA[node.string_idx];
      node.children = parlay::map(node.children, [&] (index i) {
	  // leaf if (i%2 == 0), otherwise internal
	  return (i % 2 == 0) ? 2 * SA[i/2] : i;}, 1000);});
  return result;
}

// finds a string in a suffix tree
template <typename index, typename Str, typename sStr>
long find(const radix_tree<index>& T, const Str& str, const sStr& search_str) {
  index current = T.get_root();
  index depth = 0;
  while (!T.is_leaf(current) && depth < search_str.size()) {
    index new_node = current;
    // check if any children match on first character
    for (auto child : T.get_children(current))
      if (str[T.get_string(child) + depth] == search_str[depth]) {
	new_node = child;
	break;
      }
    if (new_node == current) return -1l; // none found
    index new_depth = T.is_leaf(new_node) ? str.size() - T.get_string(new_node) : T.get_depth(new_node);
    // check that rest of characters on edge match
    for (int i = depth+1; i < new_depth; i++)
      if (i == search_str.size()) break;
      else if (str[T.get_string(new_node) + i] != search_str[i]) return -1l;
    current = new_node;
    depth = new_depth;
  }
  return current/2;
}
