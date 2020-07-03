
#ifndef PARLAY_MERGE_SORT_H_
#define PARLAY_MERGE_SORT_H_

#include "merge.h"
#include "quicksort.h"  // needed for insertion_sort
#include "utilities.h"

namespace parlay {
// not yet optimized to use moves instead of copies.

// Parallel mergesort
// This sort is stable
// if inplace is true then the output is placed in In and Out is just used
// as temp space.
template <typename InIterator, typename OutIterator, typename F>
void merge_sort_(slice<InIterator, InIterator> In,
                 slice<OutIterator, OutIterator> Out,
                 const F& f,
                 bool inplace = false) {
  size_t n = In.size();
  if (base_case(In.begin(), n / 2)) {
    parlay::insertion_sort(In.begin(), n, f);
    if (!inplace)
      for (size_t i = 0; i < n; i++)
        // copy_val<_copy>(Out[i], In[i]);
        copy_memory(Out[i], In[i]);
    return;
  }
  size_t m = n / 2;
  par_do_if(
      n > 64,
      [&]() { merge_sort_(In.cut(0, m), Out.cut(0, m), f, !inplace); },
      [&]() { merge_sort_(In.cut(m, n), Out.cut(m, n), f, !inplace); },
      true);
  if (inplace)
    internal::merge_<_copy>(Out.cut(0, m), Out.cut(m, n), In, f, true);
  else
    internal::merge_<_copy>(In.cut(0, m), In.cut(m, n), Out, f, true);
}

template <typename Iterator, class F>
void merge_sort_inplace(slice<Iterator, Iterator> In, const F& f) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto B = sequence<value_type>::uninitialized(In.size());
  merge_sort_(In, make_slice(B), f, true);
  B.clear();
}

// not the most efficent way to do due to extra copy
template <class SeqA, class F>
auto merge_sort(const SeqA& In, const F& f) {
  auto A = In;
  merge_sort_inplace(make_slice(A), f);
  return A;
}

}  // namespace parlay

#endif  // PARLAY_MERGE_SORT_H_
