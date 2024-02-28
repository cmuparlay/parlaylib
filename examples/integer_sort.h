#include <algorithm>
#include <functional>
#include <cmath>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/slice.h>
#include <parlay/delayed.h>

#include "counting_sort.h"

// **************************************************************
// Integer sort
// Does a top-down radix sort on b-bit chunks until
// the size is below a threshold (currently 16384), and then switches
// to a bottom-up radix sort.  The top down version buckets into 2^b
// buckets based on the high bits and then recurses on each bucket.
// The bottom up is a standard radix sort starting on low-order bits
// and taking advantage of the stability of each counting sort.
// Uses counting_sort for each block of the radix.
//
// Flips back and forth between in and out, with "inplace" keeping track
// of whether the result should be in in (or if false, in out).
// **************************************************************

// A bottom up radix sort with 8-bits per round
template <typename Range>
void bottom_up_radix_sort(Range in, Range out, long bits, long bot_bit, bool inplace) {
  const long radix_bits = 8;
  if (bot_bit >= bits) {
    if (!inplace) parlay::copy(in, out);
  } else {
    long num_buckets = std::min(1l << radix_bits, 1l << (bits - bot_bit));
    // the keys are bits from bot_bit to bits
    auto keys = parlay::delayed::tabulate(in.size(), [&] (long i) {
      return (in[i] >> bot_bit) & (num_buckets-1);});
    counting_sort(in.begin(), in.end(), out.begin(), keys.begin(),
		  num_buckets);
    bottom_up_radix_sort(out, in, bits, bot_bit + radix_bits, !inplace);
  }
}

template <typename Range>
void radix_sort(Range in, Range out, long bits, bool inplace) {
  long n = in.size();
  if (n == 0) return;
  const long cutoff = 16384;

  // if small do a bottom up radix sort, otherwised do top down
  if (n < cutoff) 
    bottom_up_radix_sort(in, out, bits, 0, inplace);
  else {
    // number of bits in radix 
    long radix_bits = std::min<long>(bits, std::min<long>(10l, std::ceil(std::log2(2*n/cutoff))));
    long num_buckets = 1l << radix_bits;

    // extract the needed bits for the keys
    auto keys = parlay::delayed::tabulate(n, [&] (long i) {
      return (in[i] >> bits - radix_bits) & (num_buckets-1);});
  
    // sort in into the out based on keys
    auto offsets = counting_sort(in.begin(), in.end(), out.begin(), keys.begin(),
				 num_buckets);

    // now recursively sort each bucket
    parlay::parallel_for(0, num_buckets, [&] (long i) {
      long first = offsets[i];   // start of bucket
      long last = offsets[i+1];  // end of bucket
      // note that order of in and out is flipped (as is inplace)
      radix_sort(out.cut(first,last), in.cut(first,last),
		 bits - radix_bits, !inplace);
    }, 1);
  }
}

// An inplace (i.e., result is in the same place as the input) integer_sort
// Requires O(n) temp space
template <typename Range>
void integer_sort(Range& in, long bits) {
  auto tmp = parlay::sequence<typename Range::value_type>::uninitialized(in.size());
  auto in_slice = parlay::make_slice(in);
  auto tmp_slice = parlay::make_slice(tmp);
  radix_sort(in_slice, tmp_slice, bits, true);
}
