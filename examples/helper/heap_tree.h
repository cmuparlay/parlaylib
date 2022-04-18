#pragma once

#include <parlay/sequence.h>
#include <parlay/utilities.h>

// An efficient search tree for replacing binary-search on a sorted sequence.
// Returns the rank of the first element greater than or equal to the key.
// Reorganizes the values into a "heap ordering":
// i.e. with root at 0 and the children of position i are at 2*i+1 and 2*i+2.
// Significantly more efficient than binary search when tree fits in cache
// since it avoids conditionals.
// The number of pivots must be 2^i-1 (i.e. fully balanced tree)
template <typename T>
struct heap_tree {
 private:
  const long size;
  parlay::sequence<T> tree;
  const long levels;

  // converts from sorted sequence into a "heap indexed" tree
  void to_tree(const parlay::sequence<T>& In,
               long root, long l, long r) {
    size_t n = r - l;
    size_t m = l + n / 2;
    tree[root] = In[m];
    if (n == 1) return;
    to_tree(In, 2 * root + 1, l, m);
    to_tree(In, 2 * root + 2, m + 1, r);
  }
 public:
  // constructor
  heap_tree(const parlay::sequence<T>& keys) :
      size(keys.size()), tree(parlay::sequence<T>(size)),
      levels(parlay::log2_up(keys.size()+1)-1) {
    to_tree(keys, 0, 0, size);}

  // finds a key in the "heap indexed" tree
  template <typename Less>
  inline int find(const T& key, const Less& less) {
    long j = 0;
    for (int k = 0; k < levels; k++) {
      j = 1 + 2 * j + less(tree[j],key);
    }
    j = 1 + 2 * j + !less(key,tree[j]);
    return j - size;
  }
};
