#include <algorithm>
#include <functional>

#include <parlay/parallel.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>

// **************************************************************
// Counting sort
// A parallel version of counting sort.  It breaks the input into
// partitions and for each partition, in parallel, it counts how many
// of each key there are.  It then using scan to calculate the offsets
// for each bucket in each partition, and does a final pass placing
// all keys in their correct position.
// **************************************************************

using counter_type = unsigned long;

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
  long n = end - begin;
  if (n == 0) return parlay::sequence<counter_type>(1, 0);
  long num_parts = std::min(1000l, n / (num_buckets * 64) + 1);
  long part_size = (n - 1)/num_parts + 1;

  // first count buckets within each partition
  auto counts = parlay::sequence<counter_type>::uninitialized(num_buckets * num_parts);
  parlay::parallel_for(0, num_parts, [&] (long i) {
    long start = i * part_size;
    long end = std::min<long>(start + part_size, n);
    for (long j = 0; j < num_buckets; j++) counts[i*num_buckets + j] = 0;
    for (long j = start; j < end; j++) counts[i*num_buckets + keys[j]]++;
  }, 1);

   // transpose the counts if more than one part                                                             
  parlay::sequence<counter_type> trans_counts;                                                                       
  if (num_parts > 1) {
    trans_counts = parlay::sequence<counter_type>::uninitialized(num_buckets * num_parts);
    parlay::parallel_for(0, num_buckets, [&] (long i) {
      for (size_t j = 0; j < num_parts; j++)
	trans_counts[i* num_parts + j] = counts[j * num_buckets + i];}, 1);
  } else trans_counts = std::move(counts);

  // scan for offsets for all buckets
  parlay::scan_inplace(trans_counts);

  // go back over partitions to place in final location
  parlay::parallel_for(0, num_parts, [&] (long i) {
    long start = i * part_size;
    long end = std::min<long>(start + part_size, n);
    parlay::sequence<counter_type> local_offsets(num_buckets);

    // transpose back
    for (long j = 0; j < num_buckets; j++)
       local_offsets[j] = trans_counts[num_parts * j + i];

    // copy to output
    for (long j = start; j < end; j++) {
      counter_type k = local_offsets[keys[j]]++;
      // prefetching speeds up the code
      #if defined(__GNUC__) || defined(__clang__)
      __builtin_prefetch (((char*) &out[k]) + 64);
      #endif
      out[k] = begin[j];
    }}, 1);

  return parlay::tabulate(num_buckets+1, [&] (long i) {
    return (i == num_buckets) ? (counter_type) n : trans_counts[i * num_parts];});
}

// A version that uses ranges as inputs and generates its own output sequence
template <typename InRange, typename KeysRange>
auto counting_sort(const InRange& in, const KeysRange& keys,
		   long num_buckets) {
  auto out = parlay::sequence<typename InRange::value_type>::uninitialized(in.size());
  auto offsets = counting_sort(in.begin(), in.end(), out.begin(), keys.begin(),
			       num_buckets);
  return std::pair(std::move(out), std::move(offsets));
}
