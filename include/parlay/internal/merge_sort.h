
#ifndef PARLAY_MERGE_SORT_H_
#define PARLAY_MERGE_SORT_H_

#include "merge.h"
#include "quicksort.h"  // needed for insertion_sort

#include "../utilities.h"

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
                 size_t levels,
                 bool inplace) {

  // Base case
  if (levels == 0) {
    insertion_sort(In.begin(), In.size(), f);
    if (!inplace) {
      for (size_t i = 0; i < In.size(); i++) {
        assign_uninitialized(Out[i], std::move(In[i]));
      }
    }
  }
  else {
    size_t n = In.size();
    size_t m = n / 2;
    par_do_if(
      n > 64,
      [&]() { merge_sort_(In.cut(0, m), Out.cut(0, m), f, levels-1, !inplace); },
      [&]() { merge_sort_(In.cut(m, n), Out.cut(m, n), f, levels-1, !inplace); },
    true);

    if (inplace) {
      merge_into<move_tag>(Out.cut(0, m), Out.cut(m, n), In, f);
    }
    else {
      if (levels == 1) {
        merge_into<uninitialized_move_tag>(In.cut(0, m), In.cut(m, n), Out, f);
      }
      else {
        merge_into<move_tag>(In.cut(0, m), In.cut(m, n), Out, f);
      }
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
    auto B = sequence<value_type>::uninitialized(n);
    size_t levels = n >= MERGE_SORT_BASE ? log2_up(n / MERGE_SORT_BASE) : 0;
    merge_sort_(In, make_slice(B), f, levels, true);
  }
}

// not the most efficent way to do due to extra copy
template <typename Iterator, typename BinaryOp>
auto merge_sort(slice<Iterator, Iterator> In, const BinaryOp& f) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto A = parlay::sequence<value_type>(In.begin(), In.end());
  merge_sort_inplace(make_slice(A), f);
  return A;
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_MERGE_SORT_H_
