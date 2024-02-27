#include <algorithm>
#include <functional>

#include <parlay/parallel.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>

template <typename InIt, typename OutIt, typename KeyIt>                                                    
parlay::sequence<int>                                                                                       
counting_sort(const InIt& begin, const InIt& end,                                                           
              OutIt out, const KeyIt& keys,                                                                 
              long num_buckets) {
  long n = end - begin;
  long num_parts = n / (num_buckets * 64) + 1;
  long part_size = (n - 1)/num_parts + 1;

  // first count buckets within each partition
  auto counts = parlay::sequence<int>::uninitialized(num_buckets * num_parts);
  parlay::parallel_for(0, num_parts, [&] (long i) {
    long start = i * part_size;
    long end = std::min<long>(start + part_size, n);
    for (int j = 0; j < num_buckets; j++) counts[i*num_buckets + j] = 0;
    for (size_t j = start; j < end; j++) counts[i*num_buckets + keys[j]]++;
  }, 1);

   // transpose the counts if more than one part                                                             
  parlay::sequence<int> trans_counts;                                                                       
  if (num_parts > 1) {                                                                                      
    trans_counts = parlay::sequence<int>::uninitialized(num_buckets * num_parts);                           
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
    int local_offsets[num_buckets];

    // transpose back
    for (int j = 0; j < num_buckets; j++)
       local_offsets[j] = trans_counts[num_parts * j + i];

    // copy to output
    for (size_t j = start; j < end; j++) {
      int k = local_offsets[keys[j]]++;
      __builtin_prefetch (((char*) &out[k]) + 64);
      out[k] = begin[j];
    }}, 1);

  return parlay::tabulate(num_buckets, [&] (long i) {                                                       
    return trans_counts[i * num_parts];});   
}
