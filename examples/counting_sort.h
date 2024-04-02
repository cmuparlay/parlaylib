#include <algorithm>
#include <functional>
#include <vector>

#include <parlay/parallel.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>
#include <parlay/internal/get_time.h>

// **************************************************************
// Counting sort
// A parallel version of counting sort.  It breaks the input into
// partitions and for each partition, in parallel, it counts how many
// of each key there are.  It then using scan to calculate the offsets
// for each bucket in each partition, and does a final pass placing
// all keys in their correct position.
//
// For input of size n, and for m buckets
//   Work = O(n)
//   Span = O(m + n / m)
// **************************************************************

using counter_type = unsigned long;

void prefetch(void* l) {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch (l);
#endif
}

// **************************************************************
// Input:
//   begin and end iterators for the values to be rearranged
//   begin iterator for the output (value_type must be the same)
//   begin iterator for the keys (range must be same length as values)
//   num_buckets : number of buckets (should be smallish, e.g. 256)
// Output:
//   Offsets within output of each key.  Will be of length
//   num_buckets+1 since last entry will contain total size
//   (i.e. end-begin).
// **************************************************************
template <typename InIt, typename OutIt, typename KeyIt>
parlay::sequence<counter_type>
counting_sort(const InIt& begin, const InIt& end,
              OutIt out, const KeyIt& keys,
              long num_buckets) {
  counter_type n = end - begin;
  if (n == 0) return parlay::sequence<counter_type>(1, 0);
  long num_parts = std::min<long>(1000l, n / (num_buckets * 64) + 1);
  counter_type part_n = (n - 1)/num_parts + 1;
  long m = num_buckets * num_parts;

  // first count buckets within each partition
  auto counts = parlay::sequence<counter_type>::uninitialized(m);
  parlay::parallel_for(0, num_parts, [&] (long i) {
    std::vector<counter_type> local_counts(num_buckets);
    for (long j = 0; j < num_buckets; j++)
      local_counts[j] = 0;
    for (long j = i * part_n; j < std::min((i+1) * part_n, n); j++)
      local_counts[keys[j]]++;
    // transpose so equal buckets are adjacent
    for (long j = 0; j < num_buckets; j++) 
      counts[num_parts * j + i] = local_counts[j];
  }, 1);

  // scan for offsets for all buckets
  parlay::scan_inplace(counts);

  // go back over partitions to place in final location
  parlay::parallel_for(0, num_parts, [&] (long i) {
    std::vector<counter_type> local_offsets(num_buckets);
    // transpose back into local offsets
    for (long j = 0; j < num_buckets; j++)
       local_offsets[j] = counts[num_parts * j + i];
    // copy to output
    for (long j = i * part_n; j < std::min((i+1) * part_n, n); j++) {
      counter_type k = local_offsets[keys[j]]++;
      prefetch(((char*) &out[k]) + 64);
      out[k] = begin[j];
    }}, 1);

  // return offsets for each bucket.
  // includes extra element at end containing n
  return parlay::tabulate(num_buckets + 1, [&] (long i) {
           return (i == num_buckets) ? n : counts[i * num_parts];});
}

// A wrapper that uses ranges as inputs and generates its own output
// sequence
template <typename InRange, typename KeysRange>
auto counting_sort(const InRange& in, const KeysRange& keys,
                   long num_buckets) {
  using V = typename InRange::value_type;
  auto out = parlay::sequence<V>::uninitialized(in.size());
  auto offsets = counting_sort(in.begin(), in.end(),
                               out.begin(), keys.begin(),
                               num_buckets);
  return std::pair(std::move(out), std::move(offsets));
}
