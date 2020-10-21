#ifndef PARLAY_MERGE_SORT_H_
#define PARLAY_MERGE_SORT_H_

#include "merge.h"
#include "quicksort.h"  // needed for insertion_sort

#include "../utilities.h"
#include "uninitialized_sequence.h"

namespace parlay {
namespace internal {

// Size at which to perform insertion sort instead
constexpr size_t MERGE_SORT_BASE = 48;

// Parallel mergesort
// This sort is stable
// if inplace is true then the output is placed in In and
// Out is just used as temp space.
template <typename InIterator, typename OutIterator, typename BinaryOp>
void merge_sort_(slice<InIterator, InIterator> In,
                 slice<OutIterator, OutIterator> Out,
                 const BinaryOp& f,
                 bool inplace) {
  size_t n = In.size();
  // Base case
  if (n < MERGE_SORT_BASE) {
    insertion_sort(In.begin(), In.size(), f);
    if (!inplace) {
      for (size_t i = 0; i < In.size(); i++) {
        uninitialized_relocate(&Out[i], &In[i]);
      }
    }
  }
  else {
    size_t m = n / 2;
    par_do_if(
      n > 64,
      [&]() { merge_sort_(In.cut(0, m), Out.cut(0, m), f, !inplace); },
      [&]() { merge_sort_(In.cut(m, n), Out.cut(m, n), f, !inplace); },
    true);

    if (inplace) {
      merge_into<uninitialized_relocate_tag>(Out.cut(0, m), Out.cut(m, n), In, f);
    }
    else {
      merge_into<uninitialized_relocate_tag>(In.cut(0, m), In.cut(m, n), Out, f);
    }
  }
}

template <typename Iterator, typename BinaryOp>
void merge_sort_inplace(slice<Iterator, Iterator> In, const BinaryOp& f) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  size_t n = In.size();
  if (n <= MERGE_SORT_BASE) {
    insertion_sort(In.begin(), In.size(), f);
  }
  else {
    auto B = uninitialized_sequence<value_type>(n);
    merge_sort_(In, make_slice(B), f, true);
  }
}

// not the most efficent way to do due to extra copy
template <typename Iterator, typename BinaryOp>
[[nodiscard]] auto merge_sort(slice<Iterator, Iterator> In, const BinaryOp& f) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto A = parlay::sequence<value_type>(In.begin(), In.end());
  merge_sort_inplace(make_slice(A), f);
  return A;
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_MERGE_SORT_H_
