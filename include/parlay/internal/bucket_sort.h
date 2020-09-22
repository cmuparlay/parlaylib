
#ifndef PARLAY_BUCKET_SORT_H_
#define PARLAY_BUCKET_SORT_H_

#include "merge_sort.h"
#include "quicksort.h"
#include "sequence_ops.h"

#include "../utilities.h"

namespace parlay {
namespace internal {

using uchar = unsigned char;
using uint = unsigned int;

template <typename InIterator, typename OutIterator, typename KT>
void radix_step_(slice<InIterator, InIterator> A,
                 slice<OutIterator, OutIterator> B,
                 KT* keys,
                 size_t* counts,
                 size_t m) {
  size_t n = A.size();
  for (size_t i = 0; i < m; i++) counts[i] = 0;
  for (size_t j = 0; j < n; j++) {
    size_t k = keys[j];
    counts[k]++;
  }

  size_t s = 0;
  for (size_t i = 0; i < m; i++) {
    s += counts[i];
    counts[i] = s;
  }

  for (long j = n - 1; j >= 0; j--) {
    long x = --counts[keys[j]];
    B[x] = std::move(A[j]);
  }
}

template <typename InIterator, typename OutIterator>
void to_heap_order(InIterator In,
                   OutIterator Out,
                   size_t root,
                   size_t l,
                   size_t r) {
  size_t n = r - l;
  size_t m = l + n / 2;
  Out[root] = std::move(In[m]);
  if (n == 1) return;
  to_heap_order(In, Out, 2 * root + 1, l, m);
  to_heap_order(In, Out, 2 * root + 2, m + 1, r);
}

// returns true if all equal
template <typename Iterator, typename BinaryOp>
bool get_buckets(slice<Iterator, Iterator> A,
                 uchar* buckets,
                 BinaryOp f,
                 size_t rounds) {
  size_t n = A.size();
  size_t num_buckets = (1 << rounds);
  size_t over_sample = 1 + n / (num_buckets * 400);
  size_t sample_set_size = num_buckets * over_sample;
  size_t num_pivots = num_buckets - 1;
  
  auto sample_set = sequence<size_t>::from_function(sample_set_size,
    [&](size_t i) { return hash64(i) % n; });

  // sort the samples
  quicksort(sample_set.begin(), sample_set_size, [&](size_t i, size_t j) {
    return f(A[i], A[j]); });

  auto pivots = sequence<size_t>::from_function(
      num_pivots, [&](size_t i) { return sample_set[over_sample * (i + 1)]; });

  if (!f(A[pivots[0]], A[pivots[num_pivots - 1]])) return true;

  auto heap = sample_set.begin();
  to_heap_order(pivots.begin(), heap, 0, 0, num_pivots);

  for (size_t i = 0; i < n; i++) {
    size_t j = 0;
    for (size_t k = 0; k < rounds; k++) j = 1 + 2 * j + !f(A[i], A[heap[j]]);
    buckets[i] = j - num_pivots;
  }
  return false;
}

template <typename InIterator, typename OutIterator, typename BinaryOp>
void base_sort(slice<InIterator, InIterator> in,
               slice<OutIterator, OutIterator> out,
               BinaryOp f,
               bool stable,
               bool inplace) {
  if (stable) {
    auto n = in.size();
    size_t levels = n > MERGE_SORT_BASE ? log2_up(n / MERGE_SORT_BASE) : 0;
    merge_sort_(in, out, f, levels, inplace);
  }
  else {
    quicksort(in.begin(), in.size(), f);
    if (!inplace) {
      std::copy(std::make_move_iterator(in.begin()),
                std::make_move_iterator(in.end()),
                out.begin());
    }
  }
}

template <typename InIterator, typename OutIterator, typename BinaryOp>
void bucket_sort_r(slice<InIterator, InIterator> in,
                   slice<OutIterator, OutIterator> out,
                   BinaryOp f,
                   bool stable,
                   bool inplace) {
  size_t n = in.size();
  size_t bits = 4;
  size_t num_buckets = 1 << bits;
  if (n < num_buckets * 32) {
    base_sort(in, out, f, stable, inplace);
  } else {
    auto counts = sequence<size_t>::uninitialized(num_buckets);
    auto bucketsm = sequence<uchar>::uninitialized(n);
    uchar* buckets = bucketsm.begin();
    if (get_buckets(in, buckets, f, bits)) {
      base_sort(in, out, f, stable, inplace);
    } else {
      radix_step_(in, out, buckets, counts.data(), num_buckets);
      auto loop = [&] (size_t j) {
        size_t start = counts[j];
        size_t end = (j == num_buckets - 1) ? n : counts[j + 1];
        bucket_sort_r(out.cut(start, end), in.cut(start, end), f, stable, !inplace);
      };
      parallel_for(0, num_buckets, loop, 4);
    }
  }
}

template <typename Iterator, typename BinaryOp>
void bucket_sort(slice<Iterator, Iterator> in, BinaryOp f, bool stable = false) {
  using T = typename slice<Iterator, Iterator>::value_type;
  size_t n = in.size();
  auto tmp = sequence<T>::uninitialized(n);
  bucket_sort_r(make_slice(in), make_slice(tmp), f, stable, true);
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_BUCKET_SORT_H_
