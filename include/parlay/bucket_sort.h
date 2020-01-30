
#ifndef PARLAY_BUCKET_SORT_H_
#define PARLAY_BUCKET_SORT_H_

#include <algorithm>

#include "alloc.h"
#include "sequence_ops.h"
#include "quicksort.h"
#include "utilities.h"

namespace parlay {
  
  using uchar = unsigned char;
  using uint = unsigned int;
  
  template <class T, class KT>
  void radix_step_(T* A, T* B, KT* keys, size_t* counts, size_t n, size_t m) {
    for (size_t i = 0; i < m; i++)  counts[i] = 0;
    for (size_t j = 0; j < n; j++) {
      size_t k = keys[j];
      counts[k]++;
    }

    size_t s = 0;
    for (size_t i = 0; i < m; i++) {
      s += counts[i];
      counts[i] = s;
    }

    for (long j = n-1; j >= 0; j--) {
      long x = --counts[keys[j]];
      copy_memory(B[x], A[j]);
    }
  }

  template <class T>
  void to_heap_order(T* In, T* Out,
  		   size_t root, size_t l, size_t r) {
    size_t n = r-l;
    size_t m = l + n/2;
    //copy_memory(Out[root], In[m]);
    Out[root] = In[m];
    if (n == 1) return;
    to_heap_order(In, Out, 2*root+1, l, m);
    to_heap_order(In, Out, 2*root+2, m+1, r);
  }

  // returns true if all equal
  template <class T, class binOp>
  bool get_buckets(range<T*> A, uchar* buckets, binOp f, size_t rounds) {
    size_t n = A.size();
    size_t num_buckets = (1 << rounds);
    size_t over_sample = 1 + n/(num_buckets * 400);
    size_t sample_set_size = num_buckets * over_sample;
    size_t num_pivots = num_buckets-1;
    auto sample_set = sequence<T>(sample_set_size, [&] (size_t i) {
	return A[hash64(i)%n];});

    // sort the samples
    quicksort(sample_set.begin(), sample_set_size, f);

    auto pivots = sequence<T>(num_pivots, [&] (size_t i) {
	return sample_set[over_sample*(i+1)];});

    if (!f(pivots[0],pivots[num_pivots-1])) 
      return true;

    T* heap = sample_set.begin();
    to_heap_order(pivots.begin(), heap, 0, 0, num_pivots);

    for (size_t i=0; i < n; i++) {
      size_t j = 0;
      for (size_t k=0; k < rounds; k++)
        j = 1 + 2*j + !f(A[i], heap[j]);
      buckets[i] = j-num_pivots;
    }
    return false;
  }

  template <class T, class binOp>
  void base_sort(range<T*> in, range<T*> out, binOp f,
  	       bool stable, bool inplace) {
    if (stable) merge_sort_(in, out, f, inplace);
    else {
      quicksort(in.begin(), in.size(), f);
      if (!inplace) for (size_t i=0; i < in.size(); i++)
		      copy_memory(out[i], in[i]);
    }
  }

  template <class T, class binOp>
  void bucket_sort_r(range<T*> in, range<T*> out, binOp f,
  		   bool stable, bool inplace) {
    size_t n = in.size();
    size_t bits = 4;
    size_t num_buckets = 1 << bits;
    if (n < num_buckets*32) {
      base_sort(in, out, f, stable, inplace);
    } else {
      size_t counts[num_buckets];
      sequence<uchar> bucketsm(n);
      uchar* buckets = bucketsm.begin();
      if (get_buckets(in, buckets, f, bits)) {
        base_sort(in, out, f, stable, inplace);
      } else {
        radix_step_(in.begin(), out.begin(), buckets, counts, n, num_buckets);
        auto loop = [&] (size_t j) {
  	  size_t start = counts[j];
  	  size_t end = (j == num_buckets-1) ? n : counts[j+1];
  	  bucket_sort_r(out.slice(start,end), in.slice(start,end), f,
  			stable, !inplace);
  	};
	parallel_for (0, num_buckets, loop, 4);
      }
    }
  }

  template <class T, class binOp>
  void bucket_sort(range<T*> in, binOp f, bool stable=false) {
    size_t n = in.size();
    auto tmp = sequence<T>::no_init(n);
    bucket_sort_r(in.slice(), tmp.slice(), f, stable, true);
    tmp.clear_no_destruct();
  }
}  // namespace parlay

#endif  // PARLAY_BUCKET_SORT_H_
