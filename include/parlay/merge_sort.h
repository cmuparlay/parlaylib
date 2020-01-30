
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
  template <class Iter, class F>
  void merge_sort_(range<Iter> In,
		   range<Iter> Out,
		   const F& f,
		   bool inplace=false) {
    size_t n = In.size();
    if (base_case(In.begin(), n/2)) {
      parlay::insertion_sort(In.begin(), n, f);
      if (!inplace)
	for (size_t i=0; i < n; i++)
	  //copy_val<_copy>(Out[i], In[i]);
	  copy_memory(Out[i], In[i]);
      return;
    }
    size_t m = n/2;
    par_do_if(n > 64,
	   [&] () {merge_sort_(In.slice(0,m), Out.slice(0,m), f, !inplace);},
	   [&] () {merge_sort_(In.slice(m,n), Out.slice(m,n), f, !inplace);},
	   true);
    if (inplace)
      parlay::merge_<_copy>(Out.slice(0,m), Out.slice(m,n), In, f, true);
    else
      parlay::merge_<_copy>(In.slice(0,m), In.slice(m,n), Out, f, true);
  }

  template <class T, class F>
  void merge_sort_inplace(range<T*> In, const F& f) {
    auto B = sequence<T>::no_init(In.size());
    merge_sort_(In.slice(), B.slice(), f, true);
    B.clear_no_destruct();
  }
  
  // not the most efficent way to do due to extra copy
  template <class SeqA, class F>
  sequence<typename SeqA::value_type>
  merge_sort(const SeqA &In, const F& f) {
    using T = typename SeqA::value_type;
    sequence<T> A(In);
    merge_sort_inplace(A.slice(), f);
    return A;
  }
}  // namespace parlay

#endif  // PARLAY_MERGE_SORT_H_

