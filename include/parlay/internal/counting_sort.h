
#ifndef PARLAY_COUNTING_SORT_H_
#define PARLAY_COUNTING_SORT_H_

#include <math.h>
#include <stdio.h>

#include <chrono>
#include <thread>

#include "sequence_ops.h"
#include "transpose.h"

#include "../utilities.h"

// TODO
// Make sure works for inplace or not with regards to move_uninitialized

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t SEQ_THRESHOLD = 8192;
constexpr const size_t BUCKET_FACTOR = 32;
constexpr const size_t LOW_BUCKET_FACTOR = 16;

// count number in each bucket
template <typename InSeq, typename CountIterator, typename KeySeq>
void seq_count_(InSeq In, KeySeq Keys, CountIterator counts, size_t num_buckets) {
  size_t n = In.size();
  // use local counts to avoid false sharing
  auto local_counts = sequence<size_t>(num_buckets);
  for (size_t j = 0; j < n; j++) {
    size_t k = Keys[j];
    if (k >= num_buckets)
      throw std::runtime_error("bucket out of range in count_sort");
    local_counts[k]++;
  }
  for (size_t i = 0; i < num_buckets; i++) counts[i] = local_counts[i];
}

// write to destination, where offsets give start of each bucket
template <typename copy_tag, typename out_uninitialized_tag,
          typename InSeq, typename OutIterator, typename CountIterator, typename KeySeq>
void seq_write_(InSeq In, OutIterator Out, KeySeq Keys,
                CountIterator offsets, size_t num_buckets) {
  // copy to local offsets to avoid false sharing
  auto local_offsets = sequence<size_t>::uninitialized(num_buckets);
  for (size_t i = 0; i < num_buckets; i++) local_offsets[i] = offsets[i];
  for (size_t j = 0; j < In.size(); j++) {
    size_t k = local_offsets[Keys[j]]++;
    assign_dispatch(Out[k], In[j], copy_tag(), out_uninitialized_tag());
  }
}

// write to destination, where offsets give end of each bucket
template <typename copy_tag, typename out_uninitialized_tag,
          typename InSeq, typename OutIterator, typename OffsetIterator, typename KeySeq>
void seq_write_down_(InSeq In, OutIterator Out, KeySeq Keys,
                     OffsetIterator offsets, size_t) {  // num_buckets) {
  for (long j = In.size() - 1; j >= 0; j--) {
    long k = --offsets[Keys[j]];
    assign_dispatch(Out[k], In[j], copy_tag(), out_uninitialized_tag());
  }
}

// Sequential counting sort internal
template <typename copy_tag, typename out_uninitialized_tag,
          typename InS, typename OutS, typename CountIterator, typename KeyS>
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
  seq_write_down_<copy_tag, out_uninitialized_tag>(In, Out.begin(), Keys, counts, num_buckets);
}

// Sequential counting sort
template <typename copy_tag, typename out_uninitialized_tag,
          typename InIterator, typename OutIterator, typename KeyIterator>
sequence<size_t> seq_count_sort(slice<InIterator, InIterator> In,
                                slice<OutIterator, OutIterator> Out,
                                slice<KeyIterator, KeyIterator> Keys,
                                size_t num_buckets) {
  auto counts = sequence<size_t>::uninitialized(num_buckets + 1);
  seq_count_sort_<copy_tag, out_uninitialized_tag>(In, Out, Keys, counts.begin(), num_buckets);
  counts[num_buckets] = In.size();
  return counts;
}

// Parallel internal counting sort specialized to type for bucket counts
// returns counts, and a flag
// If skip_if_in_one and returned flag is true, then the Input was alread
// sorted, and it has not been moved to the output.
template <typename copy_tag, typename out_uninitialized_tag,
          typename s_size_t, typename InIterator, typename OutIterator, typename KeyIterator>
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
  size_t par_lower = 1 + round(num_threads * parallelism * 9);
  size_t size_lower = 1;  // + n * sizeof(T) / 2000000;
  size_t bucket_upper =
      1 + n * sizeof(T) / (4 * num_buckets * sizeof(s_size_t));
  size_t num_blocks = (std::min)(bucket_upper, (std::max)(par_lower, size_lower));

  // if insufficient parallelism, sort sequentially
  if (n < SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1) {
    return std::make_pair(
      seq_count_sort<copy_tag, out_uninitialized_tag>(In, Out, Keys, num_buckets),
      false);
  }

  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  auto counts = sequence<s_size_t>::uninitialized(m);

  // sort each block
  parallel_for(0, num_blocks,
               [&](size_t i) {
                 s_size_t start = (std::min)(i * block_size, n);
                 s_size_t end = (std::min)(start + block_size, n);
                 seq_count_(In.cut(start, end), make_slice(Keys).cut(start, end),
                            counts.begin() + i * num_buckets, num_buckets);
               },
               1, is_nested);

  sequence<size_t> bucket_offsets = sequence<size_t>::uninitialized(num_buckets + 1);
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
  size_t total = scan_inplace(make_slice(bucket_offsets), addm<size_t>());
  if (skip_if_in_one && num_non_zero == 1) {
    return std::make_pair(std::move(bucket_offsets), true);
  }

  if (total != n) throw std::logic_error("in count_sort, internal bad count");

  auto dest_offsets = sequence<s_size_t>::uninitialized(num_blocks * num_buckets);
  parallel_for(0, num_buckets,
               [&](size_t i) {
                 size_t v = bucket_offsets[i];
                 size_t start = i * num_blocks;
                 for (size_t j = 0; j < num_blocks; j++) {
                   dest_offsets[start + j] = v;
                   v += counts[j * num_buckets + i];
                 }
               },
               1 + 1024 / num_blocks);

  auto counts2 = sequence<s_size_t>::uninitialized(m);

  parallel_for(0, num_blocks,
               [&](size_t i) {
                 size_t start = i * num_buckets;
                 for (size_t j = 0; j < num_buckets; j++)
                   counts2[start + j] = dest_offsets[j * num_blocks + i];
               },
               1 + 1024 / num_buckets);

  parallel_for(0, num_blocks,
               [&](size_t i) {
                 s_size_t start = (std::min)(i * block_size, n);
                 s_size_t end = (std::min)(start + block_size, n);
                 seq_write_<copy_tag, out_uninitialized_tag>(In.cut(start, end), Out.begin(),
                            make_slice(Keys).cut(start, end),
                            counts2.begin() + i * num_buckets, num_buckets);
               },
               1, is_nested);

  return std::make_pair(std::move(bucket_offsets), false);
}

// If skip_if_in_one and returned flag is true, then the Input was alread
// sorted, and it has not been moved to the output.
template <typename copy_tag, typename out_uninitialized_tag,
          typename InIterator, typename OutIterator, typename KeyIterator>
auto count_sort(slice<InIterator, InIterator> In,
                slice<OutIterator, OutIterator> Out,
                slice<KeyIterator, KeyIterator> Keys,
                size_t num_buckets,
                float parallelism = 1.0,
                bool skip_if_in_one = false) {
  size_t n = In.size();
  if (n != Out.size() || n != Keys.size())
    throw std::invalid_argument("lengths don't match in call to count_sort");
  size_t max32 = ((size_t)1) << 32;
  if (n < max32 && num_buckets < max32)
    // use 4-byte counters when larger ones not needed
    return count_sort_<copy_tag, out_uninitialized_tag, uint32_t>(In, Out, Keys, num_buckets, parallelism,
                                 skip_if_in_one);
  return count_sort_<copy_tag, out_uninitialized_tag, size_t>(In, Out, Keys, num_buckets, parallelism,
                             skip_if_in_one);
}

template <typename InIterator, typename KeyS>
auto count_sort(slice<InIterator, InIterator> In, KeyS const& Keys, size_t num_buckets) {
  using value_type = typename slice<InIterator, InIterator>::value_type;
  sequence<value_type> Out(In.size());
  auto a = count_sort<std::true_type, std::false_type>(In, Out.slice(), Keys, num_buckets);
  return std::make_pair(std::move(Out), std::move(a.first));
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_COUNTING_SORT_H_
