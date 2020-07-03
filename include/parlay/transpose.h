
#ifndef PARLAY_TRANSPOSE_H_
#define PARLAY_TRANSPOSE_H_

#include "utilities.h"

namespace parlay {

#ifdef PAR_GRANULARITY
constexpr const size_t TRANS_THRESHHOLD = PAR_GRANULARITY / 4;
#else
constexpr const size_t TRANS_THRESHHOLD = 500;
#endif

inline size_t split(size_t n) { return n / 2; }

template <class Iterator>
struct transpose {
  Iterator A, B;
  transpose(Iterator AA, Iterator BB) : A(AA), B(BB) {}

  void transR(size_t rStart, size_t rCount, size_t rLength, size_t cStart,
              size_t cCount, size_t cLength) {
    if (cCount * rCount < TRANS_THRESHHOLD) {
      for (size_t i = rStart; i < rStart + rCount; i++)
        for (size_t j = cStart; j < cStart + cCount; j++)
          B[j * cLength + i] = A[i * rLength + j];
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
      size_t l1 = split(cCount);
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
#if defined(OPENMP)
#pragma omp parallel
#pragma omp single
#endif
    transR(0, rCount, cCount, 0, cCount, rCount);
  }
};

template <class Iterator, class int_t>
struct blockTrans {
  Iterator A, B;
  int_t *OA, *OB;

  blockTrans(Iterator AA, Iterator BB, int_t *OOA, int_t *OOB)
      : A(AA), B(BB), OA(OOA), OB(OOB) {}

  void transR(size_t rStart, size_t rCount, size_t rLength, size_t cStart,
              size_t cCount, size_t cLength) {
    if (cCount * rCount < TRANS_THRESHHOLD * 16) {
      parallel_for(rStart, rStart + rCount, [&](size_t i) {
        for (size_t j = cStart; j < cStart + cCount; j++) {
          size_t sa = OA[i * rLength + j];
          size_t sb = OB[j * cLength + i];
          size_t l = OA[i * rLength + j + 1] - sa;
          for (size_t k = 0; k < l; k++) copy_memory(B[k + sb], A[k + sa]);
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
      size_t l1 = split(cCount);
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
#if defined(OPENMP)
#pragma omp parallel
#pragma omp single
#endif
    transR(0, rCount, cCount, 0, cCount, rCount);
  }
};

// Moves values from blocks to buckets
// From is sorted by key within each block, in block major
// counts is the # of keys in each bucket for each block, in block major
// From and To are of lenght n
// counts is of length num_blocks * num_buckets
// Data is memcpy'd into To avoiding initializers and overloaded =
template <typename Iterator, typename s_size_t>
sequence<size_t> transpose_buckets(Iterator From, Iterator To,
                                   sequence<s_size_t>& counts, size_t n,
                                   size_t block_size, size_t num_blocks,
                                   size_t num_buckets) {
  size_t m = num_buckets * num_blocks;
  sequence<s_size_t> dest_offsets;
  auto add = addm<s_size_t>();

  // for smaller input do non-cache oblivious version
  if (n < (1 << 22) || num_buckets <= 512 || num_blocks <= 512) {
    size_t block_bits = log2_up(num_blocks);
    size_t block_mask = num_blocks - 1;
    if ((size_t)1 << block_bits != num_blocks)
      throw std::invalid_argument(
          "in transpose_buckets: num_blocks must be a power or 2");

    // determine the destination offsets
    auto get = [&](size_t i) {
      return counts[(i >> block_bits) + num_buckets * (i & block_mask)];
    };

    // slow down?
    dest_offsets = sequence<s_size_t>::from_function(m, get);
    size_t sum = scan_inplace(make_slice(dest_offsets), add);
    if (sum != n) throw std::logic_error("in transpose, internal bad count");

    // send each key to correct location within its bucket
    auto f = [&](size_t i) {
      size_t s_offset = i * block_size;
      for (size_t j = 0; j < num_buckets; j++) {
        size_t d_offset = dest_offsets[i + num_blocks * j];
        size_t len = counts[i * num_buckets + j];
        for (size_t k = 0; k < len; k++)
          copy_memory(To[d_offset++], From[s_offset++]);
      }
    };
    parallel_for(0, num_blocks, f, 1);
  } else {  // for larger input do cache efficient transpose
    // sequence<s_size_t> source_offsets(counts,m+1);
    dest_offsets = sequence<s_size_t>(m);
    transpose<typename sequence<s_size_t>::iterator>(counts.begin(), dest_offsets.begin())
        .trans(num_blocks, num_buckets);

    // do both scans inplace
    size_t total = scan_inplace(make_slice(dest_offsets), add);
    size_t total2 = scan_inplace(make_slice(counts), add);
    if (total != n || total2 != n)
      throw std::logic_error("in transpose, internal bad count");
    counts[m] = n;

    blockTrans<Iterator, s_size_t>(From, To, counts.begin(), dest_offsets.begin())
        .trans(num_blocks, num_buckets);
  }

  // return the bucket offsets, padded with n at the end
  return sequence<size_t>::from_function(num_buckets + 1, [&](size_t i) {
    return (i == num_buckets) ? n : dest_offsets[i * num_blocks];
  });
}
}  // namespace parlay

#endif  // PARLAY_TRANSPOSE_H_
