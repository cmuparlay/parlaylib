// This file is basically the cache-oblivious sorting algorithm from:
//
// Low depth cache-oblivious algorithms.
// Guy E. Blelloch, Phillip B. Gibbons and  Harsha Vardhan Simhadri.
// Proc. ACM symposium on Parallelism in algorithms and architectures (SPAA),
// 2010

#ifndef PARLAY_SAMPLE_SORT_H_
#define PARLAY_SAMPLE_SORT_H_

#include <cmath>
#include <cstdio>
#include <cstring>

#include "bucket_sort.h"
#include "quicksort.h"
#include "sequence_ops.h"
#include "transpose.h"

#include "../utilities.h"

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t QUICKSORT_THRESHOLD = 16384;
constexpr const size_t OVER_SAMPLE = 8;

// generates counts in Sc for the number of keys in Sa between consecutive
// values of Sb
// Sa and Sb must be sorted
template <typename Iterator, typename PivotIterator, typename CountIterator, typename Compare>
void merge_seq(Iterator sA,
               PivotIterator sB,
               CountIterator sC,
               size_t lA,
               size_t lB,
               Compare f) {
  if (lA == 0 || lB == 0) return;
  auto eA = sA + lA;
  auto eB = sB + lB;
  for (size_t i = 0; i <= lB; i++) sC[i] = 0;
  while (1) {
    while (f(*sA, *sB)) {
      (*sC)++;
      if (++sA == eA) return;
    }
    sB++;
    sC++;
    if (sB == eB) break;
    if (!(f(*(sB - 1), *sB))) {
      while (!f(*sB, *sA)) {
        (*sC)++;
        if (++sA == eA) return;
      }
      sB++;
      sC++;
      if (sB == eB) break;
    }
  }
  *sC = eA - sA;
}

template <typename Iterator, typename Compare>
void seq_sort_inplace(slice<Iterator, Iterator> A, const Compare& less, bool stable) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
#if defined(OPENMP)
  quicksort_serial(A.begin(), A.size(), less);
#else
  if (((sizeof(value_type) > 8) || std::is_pointer<value_type>::value) && !stable)
    quicksort(A.begin(), A.size(), less);
  else
    bucket_sort(A, less, stable);
#endif
}

template <typename InIterator, typename OutIterator, typename Compare>
void seq_sort_(slice<InIterator, InIterator> In,
               slice<OutIterator, OutIterator> Out,
               const Compare& less,
               /* inplace */ std::false_type,
               bool stable = false) {
  size_t l = In.size();
  for (size_t j = 0; j < l; j++) assign_uninitialized(Out[j], In[j]);
  seq_sort_inplace(Out, less, stable);
}

template <typename InIterator, typename OutIterator, typename Compare>
void seq_sort_(slice<InIterator, InIterator> In,
               slice<OutIterator, OutIterator> Out,
               const Compare& less,
               /* inplace */ std::true_type,
               bool stable = false) {
  size_t l = In.size();
  for (size_t j = 0; j < l; j++) assign_uninitialized(Out[j], std::move(In[j]));
  seq_sort_inplace(Out, less, stable);
}

template <class InIterator, class OutIterator, typename Compare>
void small_sort_dispatch(slice<InIterator, InIterator> In,
                         slice<OutIterator, OutIterator> Out,
                         const Compare& less,
                         /* inplace */ std::false_type,
                         bool stable = false) {
  seq_sort_(In, Out, less, std::false_type{}, stable);
}

template <class InIterator, class OutIterator, typename Compare>
void small_sort_dispatch(slice<InIterator, InIterator> In,
                         slice<OutIterator, OutIterator>,
                         const Compare& less,
                         /* inplace */ std::true_type,
                         bool stable = false) {
  seq_sort_inplace(In, less, stable);
}

// if inplace, then In and Out must be the same, i.e. it copies back to itsefl
// if inplace the copy constructor or assignment is never called on the elements
// if not inplace, then the copy constructor is called once per element
template <typename s_size_t = size_t, typename inplace_tag,
          typename InIterator, typename OutIterator,
          typename Compare>
void sample_sort_(slice<InIterator, InIterator> In,
                  slice<OutIterator, OutIterator> Out,
                  const Compare& less,
                  bool stable = false) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  size_t n = In.size();

  if (n < QUICKSORT_THRESHOLD) {
    small_sort_dispatch(In, Out, less, inplace_tag{}, stable);
  } else {
    // The larger these are, the more comparisons are done but less
    // overhead for the transpose.
    size_t bucket_quotient = 4;
    size_t block_quotient = 4;
    if (std::is_pointer<value_type>::value) {
      bucket_quotient = 2;
      block_quotient = 3;
    } else if (sizeof(value_type) > 8) {
      bucket_quotient = 3;
      block_quotient = 3;
    }
    size_t sqrt = (size_t)ceil(pow(n, 0.5));
    size_t num_blocks = 1 << log2_up((sqrt / block_quotient) + 1);
    size_t block_size = ((n - 1) / num_blocks) + 1;
    size_t num_buckets = (sqrt / bucket_quotient) + 1;
    size_t sample_set_size = num_buckets * OVER_SAMPLE;
    size_t m = num_blocks * num_buckets;

    // generate "random" samples with oversampling
    auto sample_set = sequence<value_type>::from_function(sample_set_size,
                           [&](size_t i) { return In[hash64(i) % n]; });

    // sort the samples
    quicksort(sample_set.begin(), sample_set_size, less);

    // subselect samples at even stride
    auto pivots = sequence<value_type>::from_function(num_buckets - 1,
                       [&](size_t i) { return sample_set[OVER_SAMPLE * i]; });

    sequence<value_type> Tmp = sequence<value_type>::uninitialized(n);

    // sort each block and merge with samples to get counts for each bucket
    auto counts = sequence<s_size_t>::uninitialized(m + 1);
    counts[m] = 0;
    sliced_for(n, block_size, [&](size_t i, size_t start, size_t end) {
      seq_sort_(In.cut(start, end), make_slice(Tmp).cut(start, end), less, inplace_tag{},
                stable);
      merge_seq(Tmp.begin() + start, pivots.begin(),
                counts.begin() + i * num_buckets, end - start, num_buckets - 1,
                less);
    });

    // move data from blocks to buckets
    auto bucket_offsets =
        transpose_buckets(Tmp.begin(), Out.begin(), counts, n, block_size,
                          num_blocks, num_buckets);
    Tmp.clear();

    // sort within each bucket
    parallel_for(0, num_buckets, [&](size_t i) {
      size_t start = bucket_offsets[i];
      size_t end = bucket_offsets[i + 1];

       // buckets need not be sorted if two consecutive pivots are equal
       if (i == 0 || i == num_buckets - 1 || less(pivots[i - 1], pivots[i])) {
         seq_sort_inplace(Out.cut(start, end), less, stable);
       }
     }, 1);
  }
}

template <typename Iterator, typename Compare>
auto sample_sort(slice<Iterator, Iterator> A,
                 const Compare& less,
                 bool stable = false) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  sequence<value_type> R = sequence<value_type>::uninitialized(A.size());
  if (A.size() < (std::numeric_limits<unsigned int>::max)())
    sample_sort_<unsigned int, std::false_type>(A, make_slice(R), less, stable);
  else
    sample_sort_<unsigned long long, std::false_type>(A, make_slice(R), less, stable);
  return R;
}

template <class Iterator, typename Compare>
void sample_sort_inplace(slice<Iterator, Iterator> A,
                         const Compare& less,
                         bool stable = false) {
  if (A.size() < (std::numeric_limits<unsigned int>::max)()) {
    sample_sort_<unsigned int, std::true_type>(A, A, less, stable);
  }
  else {
    sample_sort_<unsigned long long, std::true_type>(A, A, less, stable);
  }
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_SAMPLE_SORT_H_
