
#ifndef PARLAY_INTERNAL_TRANSPOSE_H_
#define PARLAY_INTERNAL_TRANSPOSE_H_

#include <cassert>
#include <cstddef>

#include "../monoid.h"
#include "../parallel.h"
#include "../sequence.h"
#include "../slice.h"
#include "../utilities.h"

#include "sequence_ops.h"

namespace parlay {
namespace internal {

#ifdef PAR_GRANULARITY
constexpr const size_t TRANS_THRESHHOLD = PAR_GRANULARITY / 4;
#else
constexpr const size_t TRANS_THRESHHOLD = 500;
#endif

#ifdef DEBUG
constexpr const size_t NON_CACHE_OBLIVIOUS_THRESHOLD = 10000;
#else
constexpr const size_t NON_CACHE_OBLIVIOUS_THRESHOLD = 1 << 22;
#endif

inline size_t split(size_t n) { return n / 2; }

// Given a flat matrix represented in row-major order (i.e., a matrix where
// each row is written one after the other in a 1D sequence), computes the
// transpose of that matrix.
//
// E.g., given [1,2,3,1,2,3,1,2,3] which represents the matrix
//
//  1 2 3
//  1 2 3
//  1 2 3
//
// it computes [1,1,1,2,2,2,3,3,3] which represents the transpose matrix
//
//  1 1 1
//  2 2 2
//  3 3 3
//
// Alternatively, you can think of it as swapping to column-major
// representation when starting from row-major representation
//
template <typename assignment_tag, typename InIterator, typename OutIterator>
struct transpose {
  InIterator In;
  OutIterator Out;
  transpose(InIterator In_, OutIterator Out_) : In(std::move(In_)), Out(std::move(Out_)) {}

  void transR(size_t rStart, size_t rCount, size_t rLength, size_t cStart,
              size_t cCount, size_t cLength) {
    if (cCount * rCount < TRANS_THRESHHOLD) {
      for (size_t i = rStart; i < rStart + rCount; i++)
        for (size_t j = cStart; j < cStart + cCount; j++)
          assign_dispatch(Out[j * cLength + i], In[i * rLength + j], assignment_tag());
    } else if (cCount > rCount) {
      size_t l1 = split(cCount);
      size_t l2 = cCount - l1;
      auto left = [&]() {
        transR(rStart, rCount, rLength, cStart, l1, cLength);
      };
      auto right = [&]() {
        transR(rStart, rCount, rLength, cStart + l1, l2, cLength);
      };
      par_do(left, right);
    } else {
      size_t l1 = split(rCount);
      size_t l2 = rCount - l1;
      auto left = [&]() {
        transR(rStart, l1, rLength, cStart, cCount, cLength);
      };
      auto right = [&]() {
        transR(rStart + l1, l2, rLength, cStart, cCount, cLength);
      };
      par_do(left, right);
    }
  }

  void trans(size_t rCount, size_t cCount) {
    transR(0, rCount, cCount, 0, cCount, rCount);
  }
};

// Given a flat matrix represented in row-major order, in which the rows are divided
// into contiguous chunks, computes the matrix resulting from transposing those chunks,
// in row-major order. Note that as the matrix is represented in row-major order, it
// may have rows of different lengths, so it might not be a real matrix.
//
// For example, consider the following matrix in which the rows are chunked
//
// [ ( 1  2) ( 3  4  5) ( 6  7  8  9) ]
// [ (10 11) (12 13 14) (15 16 17 18) ]
//
// Its block-transpose is
//
// [ ( 1  2) (10 11) ]
// [ ( 3  4  5) (12 13 14) ]
// [ (6  7  8  9) (15 16 17 18) ]
//
// (which of course is represented in row-major order because it isn't a real matrix
// as it has imbalanced rows)
//
// The input to the problem is given in the form of the input matrix in row-major order,
// the output destination, and the offsets that define where each chunk begins in the
// input and output. For example, the input offsets for the matrix above are
//
// [ 0 2 5 ]
// [ 0 2 5 ]
//
// again, given in row-major order. You can think of the offsets as the prefix sum of
// the chunk sizes.
//
template <typename assignment_tag, typename InIterator, typename OutIterator, typename CountIterator, typename DestIterator>
struct blockTrans {
  InIterator In;
  OutIterator Out;
  CountIterator InOffset;
  DestIterator OutOffset;

  blockTrans(InIterator In_, OutIterator Out_, CountIterator InOffset_, DestIterator OutOffset_)
      : In(std::move(In_)), Out(std::move(Out_)), InOffset(std::move(InOffset_)), OutOffset(std::move(OutOffset_)) {}

  void transR(size_t rStart, size_t rCount, size_t rLength, size_t cStart,
              size_t cCount, size_t cLength) {
    if (cCount * rCount < TRANS_THRESHHOLD * 16) {
      parallel_for(rStart, rStart + rCount, [&](size_t i) {
        for (size_t j = cStart; j < cStart + cCount; j++) {
          size_t sa = InOffset[i * rLength + j];
          size_t sb = OutOffset[j * cLength + i];
          size_t l = InOffset[i * rLength + j + 1] - sa;
          for (size_t k = 0; k < l; k++) {
            assign_dispatch(Out[k + sb], In[k + sa], assignment_tag());
          }
        }
      });
    } else if (cCount > rCount) {
      size_t l1 = split(cCount);
      size_t l2 = cCount - l1;
      auto left = [&]() {
        transR(rStart, rCount, rLength, cStart, l1, cLength);
      };
      auto right = [&]() {
        transR(rStart, rCount, rLength, cStart + l1, l2, cLength);
      };
      par_do(left, right);
    } else {
      size_t l1 = split(rCount);
      size_t l2 = rCount - l1;
      auto left = [&]() {
        transR(rStart, l1, rLength, cStart, cCount, cLength);
      };
      auto right = [&]() {
        transR(rStart + l1, l2, rLength, cStart, cCount, cLength);
      };
      par_do(left, right);
    }
  }

  void trans(size_t rCount, size_t cCount) {
    transR(0, rCount, cCount, 0, cCount, rCount);
  }
};

// Moves values from blocks to buckets
// From is sorted by key within each block, in block major
// counts is the # of keys in each bucket for each block, in block major
// From and To are of length n
// counts is of length num_blocks * num_buckets
//
template <typename assignment_tag, typename InIterator, typename OutIterator, typename s_size_t>
sequence<size_t> transpose_buckets(InIterator From, OutIterator To,
                                   sequence<s_size_t>& counts, size_t n,
                                   size_t block_size, size_t num_blocks,
                                   size_t num_buckets) {
  size_t m = num_buckets * num_blocks;
  sequence<s_size_t> dest_offsets;
  auto add = plus<s_size_t>();

  // for smaller input do non-cache oblivious version
  if (n < NON_CACHE_OBLIVIOUS_THRESHOLD || num_buckets <= 512 || num_blocks <= 512) {
    size_t block_bits = log2_up(num_blocks);
    size_t block_mask = num_blocks - 1;
    assert(size_t{1} << block_bits == num_blocks);

    // determine the destination offsets
    auto get = [&](size_t i) {
      return counts[(i >> block_bits) + num_buckets * (i & block_mask)];
    };

    // slow down?
    dest_offsets = sequence<s_size_t>::from_function(m, get);
    [[maybe_unused]] auto sum = scan_inplace(make_slice(dest_offsets), add);
    assert(sum == n);

    // send each key to correct location within its bucket
    auto f = [&](size_t i) {
      size_t s_offset = i * block_size;
      for (size_t j = 0; j < num_buckets; j++) {
        size_t d_offset = dest_offsets[i + num_blocks * j];
        size_t len = counts[i * num_buckets + j];
        for (size_t k = 0; k < len; k++)
          assign_dispatch(To[d_offset++], From[s_offset++], assignment_tag());
      }
    };
    parallel_for(0, num_blocks, f, 1);
  } else {  // for larger input do cache efficient transpose
    dest_offsets = sequence<s_size_t>::uninitialized(m);
    transpose<uninitialized_copy_tag, decltype(counts.begin()), decltype(dest_offsets.begin())>
      (counts.begin(), dest_offsets.begin()).trans(num_blocks, num_buckets);

    // do both scans inplace
    [[maybe_unused]] size_t total = scan_inplace(make_slice(dest_offsets), add);
    [[maybe_unused]] size_t total2 = scan_inplace(make_slice(counts), add);
    assert(total == n && total2 == n);

    counts[m] = static_cast<s_size_t>(n);

    blockTrans<assignment_tag, InIterator, OutIterator,
               typename sequence<s_size_t>::iterator,
               typename sequence<s_size_t>::iterator>(
      From, To, counts.begin(), dest_offsets.begin())
        .trans(num_blocks, num_buckets);
  }

  // return the bucket offsets, padded with n at the end
  return sequence<size_t>::from_function(num_buckets + 1, [&](size_t i) {
    return (i == num_buckets) ? n : dest_offsets[i * num_blocks];
  });
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_TRANSPOSE_H_
