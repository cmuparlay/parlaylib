
#ifndef PARLAY_COUNTING_SORT_H_
#define PARLAY_COUNTING_SORT_H_

#include <cassert>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#include "sequence_ops.h"
#include "uninitialized_sequence.h"

#include "../monoid.h"
#include "../parallel.h"
#include "../sequence.h"
#include "../slice.h"
#include "../utilities.h"


namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t SEQ_THRESHOLD = 8192;
constexpr const size_t BUCKET_FACTOR = 32;
constexpr const size_t LOW_BUCKET_FACTOR = 16;

// count number in each bucket
template <typename InSeq, typename CountIterator, typename KeySeq>
void seq_count_(InSeq In, KeySeq Keys, CountIterator counts, size_t num_buckets) {
  using s_size_t = typename std::iterator_traits<CountIterator>::value_type;
  size_t n = In.size();
  // use local counts to avoid false sharing
  auto local_counts = sequence<s_size_t>(num_buckets);
  for (size_t j = 0; j < n; j++) {
    size_t k = Keys[j];
    assert(k < num_buckets);
    local_counts[k]++;
  }
  for (size_t i = 0; i < num_buckets; i++) counts[i] = local_counts[i];
}

// write to destination, where offsets give start of each bucket
template <typename assignment_tag, typename InSeq, typename OffsetIterator, typename KeySeq>
void seq_write_(InSeq In, KeySeq Keys, OffsetIterator offsets, size_t num_buckets) {
  // copy to local offsets to avoid false sharing
  using oi = typename std::iterator_traits<OffsetIterator>::value_type;
  auto local_offsets = sequence<oi>::uninitialized(num_buckets);
  for (size_t i = 0; i < num_buckets; i++) local_offsets[i] = offsets[i];
  for (size_t j = 0; j < In.size(); j++) {
    oi k = local_offsets[Keys[j]]++;
    // needs to be made portable
    #if defined(__GNUC__) || defined(__clang__)
    if constexpr (is_contiguous_iterator_v<oi>)
       __builtin_prefetch (((char*) k) + 64);
    #endif
    assign_dispatch(*k, In[j], assignment_tag());
  }
}

// write to destination, where offsets give end of each bucket
template <typename assignment_tag, typename InSeq, typename OutIterator, typename OffsetIterator, typename KeySeq>
void seq_write_down_(InSeq In, OutIterator Out, KeySeq Keys,
                     OffsetIterator offsets, size_t) {  // num_buckets) {
  for (std::ptrdiff_t j = In.size() - 1; j >= 0; j--) {
    auto k = --offsets[Keys[j]];
    assign_dispatch(Out[k], In[j], assignment_tag());
  }
}

// Sequential counting sort internal
template <typename assignment_tag, typename InS, typename OutS, typename CountIterator, typename KeyS>
void seq_count_sort_(InS In, OutS Out, KeyS Keys, CountIterator counts,
                     size_t num_buckets) {
  // count size of each bucket
  seq_count_(In, Keys, counts, num_buckets);

  // generate offsets for buckets
  size_t s = 0;
  for (size_t i = 0; i < num_buckets; i++) {
    s += counts[i];
    counts[i] = s;
  }

  // send to destination
  seq_write_down_<assignment_tag>(In, Out.begin(), Keys, counts, num_buckets);
}

// Sequential counting sort
template <typename assignment_tag, typename InIterator, typename OutIterator, typename KeyIterator>
sequence<size_t> seq_count_sort(slice<InIterator, InIterator> In,
                                slice<OutIterator, OutIterator> Out,
                                slice<KeyIterator, KeyIterator> Keys,
                                size_t num_buckets) {
  auto counts = sequence<size_t>::uninitialized(num_buckets + 1);
  seq_count_sort_<assignment_tag>(In, Out, Keys, counts.begin(), num_buckets);
  counts[num_buckets] = In.size();
  return counts;
}

// Parallel internal counting sort specialized to type for bucket counts
// returns counts, and a flag
// If skip_if_in_one and returned flag is true, then the Input was alread
// sorted, and it has not been moved to the output.
//
// Values are transferred from In to Out as per the type of assignment_tag.
// E.g. If assignment_tag is parlay::copy_assign_tag, values are copied,
// if it is parlay::uninitialized_move_tag, they are moved assuming that
// Out is uninitialized, etc.
template <typename assignment_tag, typename s_size_t, typename InIterator, typename OutIterator, typename KeyIterator>
std::pair<sequence<size_t>, bool> count_sort_(slice<InIterator, InIterator> In,
                                              slice<OutIterator, OutIterator> Out,
                                              slice<KeyIterator, KeyIterator> Keys,
                                              size_t num_buckets,
                                              float parallelism = 1.0,
                                              bool skip_if_in_one = false) {
  using T = typename slice<InIterator, InIterator>::value_type;
  size_t n = In.size();
  size_t num_threads = num_workers();
  bool is_nested = parallelism < .5;

  // pick number of blocks for sufficient parallelism but to make sure
  // cost on counts is not to high (i.e. bucket upper).
  // size_t par_lower = 1 + static_cast<size_t>(round(num_threads * parallelism * 9));
  // size_t size_lower = 1;  // + n * sizeof(T) / 2000000;
  // size_t bucket_upper =
  //     1 + n * sizeof(T) / (4 * num_buckets * sizeof(s_size_t));
  // size_t num_blocks = (std::min)(bucket_upper, (std::max)(par_lower, size_lower));
  size_t num_blocks = 1 + n * sizeof(T) / std::max<size_t>(num_buckets * 500, 5000);
  
  // if insufficient parallelism, sort sequentially
  if (n < SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1) {
    return std::make_pair(
      seq_count_sort<assignment_tag>(In, Out, Keys, num_buckets),
      false);
  }

  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  auto counts = sequence<s_size_t>::uninitialized(m);

  // sort each block
  parallel_for(0, num_blocks,
               [&](size_t i) {
                 size_t start = (std::min)(i * block_size, n);
                 size_t end = (std::min)(start + block_size, n);
                 seq_count_(In.cut(start, end), make_slice(Keys).cut(start, end),
                            counts.begin() + i * num_buckets, num_buckets);
               },
               1, is_nested);

  auto bucket_offsets = sequence<size_t>::uninitialized(num_buckets + 1);
  parallel_for(0, num_buckets,
               [&](size_t i) {
                 size_t v = 0;
                 for (size_t j = 0; j < num_blocks; j++)
                   v += counts[j * num_buckets + i];
                 bucket_offsets[i] = v;
               },
               1 + 1024 / num_blocks);
  bucket_offsets[num_buckets] = 0;

  // if all in one bucket, then no need to sort
  size_t num_non_zero = 0;
  for (size_t i = 0; i < num_buckets; i++)
    num_non_zero += (bucket_offsets[i] > 0);
  [[maybe_unused]] size_t total = scan_inplace(make_slice(bucket_offsets), plus<size_t>());
  if (skip_if_in_one && num_non_zero == 1) {
    return std::make_pair(std::move(bucket_offsets), true);
  }

  assert(total == n);

  auto dest_offsets = sequence<OutIterator>::uninitialized(num_blocks * num_buckets);
  parallel_for(0, num_buckets,
               [&](size_t i) {
		 auto v = bucket_offsets[i] + Out.begin();
                 for (size_t j = 0; j < num_blocks; j++) {
                   dest_offsets[j * num_buckets + i] = v;
                   v += counts[j * num_buckets + i];
                 }
               },
               1 + 1024 / num_blocks);
  
  parallel_for(0, num_blocks,
               [&](size_t i) {
                 size_t start = (std::min)(i * block_size, n);
                 size_t end = (std::min)(start + block_size, n);
                 seq_write_<assignment_tag>(In.cut(start, end), Keys.cut(start, end),
					    dest_offsets.begin() + i * num_buckets,
					    num_buckets);
               },
               1, is_nested);

  return std::make_pair(std::move(bucket_offsets), false);
}

template <typename InIterator, typename KeyIterator>
auto group_by_small_int(slice<InIterator, InIterator> In,
			slice<KeyIterator, KeyIterator> Keys,
			size_t num_buckets) {
  using T = typename slice<InIterator, InIterator>::value_type;
  size_t n = In.size();
  using s_size_t = size_t;

  size_t num_blocks = 1 + n * sizeof(T) / std::max<size_t>(num_buckets * 500, 5000);
  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  if (num_buckets == 2) {
    sequence<size_t> Sums(num_blocks);
    sliced_for(n, block_size, [&](size_t i, size_t s, size_t e) {
	size_t c = 0;
	for (size_t j = s; j < e; j++) c += (Keys[j] == 0);
	Sums[i] = c; });
    size_t m = scan_inplace(make_slice(Sums), plus<size_t>());
    auto r0 = sequence<T>::uninitialized(m);
    auto r1 = sequence<T>::uninitialized(n-m);
    sliced_for(n, block_size, [&](size_t i, size_t s, size_t e) {
	size_t c0 = Sums[i];
	size_t c1 = s - c0;
	for (size_t j = s; j < e; j++) {
	  if (Keys[j] == 0)
	    assign_uninitialized(r0[c0++], In[j]);
	  else
	    assign_uninitialized(r1[c1++], In[j]);
	}});
    return parlay::sequence<sequence<T>>{std::move(r0), std::move(r1)};
  }

  auto counts = sequence<s_size_t>::uninitialized(m);

  // sort each block
  parallel_for(0, num_blocks,
               [&](size_t i) {
                 size_t start = (std::min)(i * block_size, n);
                 size_t end = (std::min)(start + block_size, n);
                 seq_count_(In.cut(start, end), make_slice(Keys).cut(start, end),
                            counts.begin() + i * num_buckets, num_buckets);
               },
               1);

  auto total_counts = sequence<size_t>::uninitialized(num_buckets);
  parallel_for(0, num_buckets, [&](size_t i) {
    size_t v = 0;
    for (size_t j = 0; j < num_blocks; j++)
      v += counts[j * num_buckets + i];
    total_counts[i] = v;
  }, 1 + 1024 / num_blocks);
  //total_counts[num_buckets] = 0;

  auto dest_offsets = sequence<T*>::uninitialized(num_blocks * num_buckets);
  auto results = map(total_counts, [](size_t cnt) { return sequence<T>::uninitialized(cnt); });

  parallel_for(0, num_buckets, [&](size_t i) {
    auto v = results[i].begin();
    for (size_t j = 0; j < num_blocks; j++) {
      dest_offsets[j * num_buckets + i] = v;
      v += counts[j * num_buckets + i];
    }
  }, 1 + 1024 / num_blocks);

  parallel_for(0, num_blocks, [&] (size_t i) {
    size_t start = (std::min)(i * block_size, n);
    size_t end = (std::min)(start + block_size, n);
    seq_write_<uninitialized_copy_tag>(In.cut(start, end), Keys.cut(start, end),
         dest_offsets.begin() + i * num_buckets,
         num_buckets);
  }, 1);

  return results;
}

// If skip_if_in_one and returned flag is true, then the Input was alread
// sorted, and it has not been moved to the output.
//
// Values are transferred from In to Out as per the type of assignment_tag.
// E.g. If assignment_tag is parlay::copy_assign_tag, values are copied,
// if it is parlay::uninitialized_move_tag, they are moved assuming that
// Out is uninitialized, etc. assignment_tag can be uninitialized_relocate_tag,
// in which case the inputs are destructively moved, leaving the input
// as uninitialized memory that must not be destroyed.
template <typename assignment_tag, typename InIterator, typename OutIterator, typename KeyIterator>
auto count_sort(slice<InIterator, InIterator> In,
                slice<OutIterator, OutIterator> Out,
                slice<KeyIterator, KeyIterator> Keys,
                size_t num_buckets,
                float parallelism = 1.0,
                bool skip_if_in_one = false) {
  size_t n = In.size();
  assert(n == Out.size());
  assert(n == Keys.size());

  size_t max32 = static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
  if (n < max32 && num_buckets < max32)
    // use 4-byte counters when larger ones not needed
    return count_sort_<assignment_tag, uint32_t>(In, Out, Keys, num_buckets, parallelism,
                                 skip_if_in_one);
  return count_sort_<assignment_tag, size_t>(In, Out, Keys, num_buckets, parallelism,
                             skip_if_in_one);
}

template <typename InIterator, typename KeyS>
auto count_sort(slice<InIterator, InIterator> In, KeyS const& Keys, size_t num_buckets) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  auto Out = sequence<value_type>::uninitialized(In.size());
  auto a = count_sort<uninitialized_copy_tag>(In, make_slice(Out), make_slice(Keys), num_buckets);
  return std::make_pair(std::move(Out), std::move(a.first));
}

template <typename InIterator, typename KeyS>
auto count_sort_inplace(slice<InIterator, InIterator> In, KeyS const& Keys, size_t num_buckets) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  auto Tmp = uninitialized_sequence<value_type>(In.size());
  auto a = count_sort<uninitialized_relocate_tag>(In, make_slice(Tmp), make_slice(Keys), num_buckets);
  uninitialized_relocate_n(In.begin(), Tmp.begin(), In.size());
  return a.first;
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_COUNTING_SORT_H_
