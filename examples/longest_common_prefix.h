#include <parlay/sequence.h>
#include <parlay/primitives.h>
#include <parlay/internal/get_time.h>
#include "range_min.h"

// **************************************************************
// Longest common prefix
// For a sequence s and suffix array SA over the sequence with indices
// into s, generates the lenght of the longest common prefix between
// each adjacent pair in the sorted suffix array.
// Uses doubling and does O(sum_i (log(lcp[i]))) work assuming
// work-efficient range minima structure.
// Worst case is O(n log n), but often O(n) in practice.
// Span is O(polylog n).
// **************************************************************

template <class Seq1, class Seq2>
auto lcp(Seq1 const &s, Seq2 const &SA) {
  using index = typename Seq2::value_type;
  long len = 100;
  long n = SA.size();

  auto remain = to_sequence(parlay::iota<index>(n-1));
  parlay::sequence<index> L(n-1,n);
  long work=0, offset = 0;

  do {
    work += remain.size();
    // compare next len characters of adjacent strings from SA.
    auto keep = parlay::map(remain, [&] (long i) {
      long j = offset;
      auto ssa = s.begin() + SA[i];
      auto ssb = s.begin() + SA[i+1];
      long max_j = (std::min<long>)(len + offset, n - SA[i]);
      while (j < max_j && (ssa[j] == ssb[j])) j++;
      if (j < len + offset) {L[i] = j; return false;}
      return true;});
    remain = parlay::pack(remain, keep);
    offset += len;
    if (remain.size() == 0) return L;
  } while (work < 2*n); // keep doing naive search if not too much work

  // an inverse permutation for SA
  parlay::sequence<index> ISA(n);
  parlay::parallel_for(0, n, [&] (long i) {ISA[SA[i]] = i;});

  // repeatedly double offset determining LCP by joining next len chars
  // invariants: before i^th round
  //   -- L contains correct LCPs less than offset and n for the rest of them
  //   -- remain holds indices of the rest of them (i.e., LCP[i] >= offset)
  // After round, offset = 2*offset and invariant holds for the new offset
  do {
    auto rq = range_min(L, std::less<index>(), 128);

    // see if next offset chars resolves LCP
    // set L for those that do, and keep those that do not for next round
    remain = parlay::filter(remain, [&] (long i) {
      if (SA[i] + offset >= n) {L[i] = offset; return false;};
      long i1 = ISA[SA[i]+offset];
      long i2 = ISA[SA[i+1]+offset];
      long l = L[rq.query(i1, i2-1)];
      if (l < offset) {L[i] = offset + l; return false;}
      else return true;
    });
    offset *= 2;
  } while (remain.size() > 0);
  return L;
}


