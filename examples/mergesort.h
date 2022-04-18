#include <algorithm>
#include <functional>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Parallel Mergesort
// Uses two sequences and copies back and forth
// **************************************************************

// **************************************************************
// A parallel merge that merges in1 and in2 into out
// Uses divide-and-conquer
// Does O(n1 + n2) work and O(log^2 (n1 + n2)) span
// **************************************************************
template <typename Range, typename Less>
void merge(Range in1, Range in2, Range out, Less less) {
  long n1 = in1.size();
  long n2 = in2.size();
  if (n1 + n2 < 1000) std::merge(in1.begin(), in1.end(),
                                 in2.begin(), in2.end(),
                                 out.begin(), less);
  else if (n1 == 0) { parlay::copy(in2, out); }
  else if (n2 == 0) { parlay::copy(in1, out); }
  else if (n1 < n2) merge(in2, in1, out, less);
  else {
    long mid2 = std::lower_bound(in2.begin(),in2.end(),in1[n1/2])-in2.begin();
    parlay::par_do(
        [&]() { merge(in1.cut(0, n1/2), in2.cut(0, mid2),
                      out.cut(0, n1/2 + mid2), less); },
        [&]() { merge(in1.cut(n1/2, n1), in2.cut(mid2,n2),
                      out.cut(n1/2 + mid2, n1 + n2), less); });
  }
}

// **************************************************************
// A mergesort that sorts "in" into either itself of "out" depending
// on the value of inplace.
// Out can be mutated even if inplace=true
// **************************************************************
template <typename Range, typename Less>
void merge_sort_(Range in, Range out, bool inplace, Less less) {
  long n = in.size();
  if (n < 100) {
    std::stable_sort(in.begin(), in.end(), less);
    if (!inplace) parlay::copy(in, out);
  } else {
    parlay::par_do(
        [&] () {merge_sort_(in.cut(0, n/2), out.cut(0, n/2), !inplace, less);},
        [&] () {merge_sort_(in.cut(n/2, n), out.cut(n/2, n), !inplace, less);});
    if (inplace)
      merge(out.cut(0,n/2), out.cut(n/2, n), in.cut(0, n), less);
    else merge(in.cut(0,n/2), in.cut(n/2, n), out.cut(0, n), less);
  }
}

// **************************************************************
// An inplace mergesort
// Uses std::less<keytype>{} by default, but can be specified
// **************************************************************
template <typename Range, typename Less = std::less<>>
void merge_sort(Range& in, Less less = {}) {
  long n = in.size();
  using T = typename Range::value_type;
  parlay::sequence<T> tmp(n);
  merge_sort_(in.cut(0,n), tmp.cut(0,n), true, less);
}
