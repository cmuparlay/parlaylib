#include <algorithm>
#include <functional>

#include <parlay/parallel.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>

// **************************************************************
// A parallel counting sort
// Works well for a smallish (e.g. up to 256 or perhaps 1000) buckets.
// Input is a sequence of values, and a range of keys (of equal length).
// They could be the same, or the keys could be a field from the values.
// Must also specify the number of buckets and the number of paritions.
// **************************************************************

template <typename T, typename Keys>
parlay::sequence<T> counting_sort(const parlay::sequence<T>& in, const Keys& keys,
				  long num_buckets, long num_parts) {
  long n = in.size();
  long part_size = (n - 1)/num_parts + 1;

  // For each partition count number of each of the key values 
  auto all_counts = parlay::tabulate(num_parts, [&] (long i) {
    long start = i * part_size;
    long end = std::min<long>(start + part_size, n);
    parlay::sequence<int> local_counts(num_buckets, 0);
    for (size_t j = start; j < end; j++) local_counts[keys[j]]++;
    return local_counts;}, 1);

  // need to transpose the counts for the scan
  auto counts = parlay::sequence<int>::uninitialized(num_buckets * num_parts);
  parlay::parallel_for(0, num_buckets, [&] (long i) {
    for (size_t j = 0; j < num_parts; j++)
      counts[i* num_parts + j] = all_counts[j][i];}, 1);
  all_counts.clear();

  // scan for offsets for all buckets
  parlay::scan_inplace(counts);

  // the ouput sequence
  auto out = parlay::sequence<T>::uninitialized(n);
  
  // go back over partitions to place the input in final location
  parlay::parallel_for(0, num_parts, [&] (long i) {
    long start = i * part_size;
    long end = std::min<long>(start + part_size, n);
    parlay::sequence<int> local_offsets(num_buckets, 0);

    // transpose back
    for (int j = 0; j < num_buckets; j++)
       local_offsets[j] = counts[num_parts * j + i];

    // copy to output
    for (size_t j = start; j < end; j++) {
      int k = local_offsets[keys[j]]++;
      // the following line helps performance
      __builtin_prefetch (((char*) &out[k]) + 64);
      out[k] = in[j];
    }}, 1);
  return out;
}
