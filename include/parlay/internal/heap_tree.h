#include <cstddef>

#include <functional>
#include <type_traits>   // IWYU pragma: keep

#include "../sequence.h"
#include "../utilities.h"

namespace parlay {
namespace internal {

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
  size_t size;
  sequence<T> tree;
  size_t levels;

  // converts from sorted sequence into a "heap indexed" tree
  void to_tree(const sequence<T>& In, size_t root, size_t l, size_t r) {
    size_t n = r - l;
    size_t m = l + n / 2;
    tree[root] = In[m];
    if (n == 1) return;
    to_tree(In, 2 * root + 1, l, m);
    to_tree(In, 2 * root + 2, m + 1, r);
  }

 public:

  explicit heap_tree(const sequence<T>& keys) :
      size(keys.size()), tree(size),
      levels(log2_up(keys.size()+1)-1) {

    to_tree(keys, 0, 0, size);
  }

  // finds the rank of the given key
  template <typename Less = std::less<>>
  size_t rank(const T& key, Less&& less = {}) {
    static_assert(std::is_invocable_r_v<bool, Less, T, T>);
    size_t j = 0;
    for (size_t k = 0; k < levels; k++) {
      j = 1 + 2 * j + less(tree[j],key);
    }
    j = 1 + 2 * j + !less(key,tree[j]);
    return j - size;
  }
};

}  // namespace internal
}  // namespace parlay
