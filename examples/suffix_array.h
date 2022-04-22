#include <cmath>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

// **************************************************************
// Suffix Array.
// Input must be a sequence of unsigned integer type.  Uses a modified
// and optimized version of the algorithm from:
//   Apostolico, Iliopoulos, Landau, Schieber, and Vishkin.
//   Optimal parallel suffix tree construction.
//   STOC '94.
// The work is O(n log n) work in the worst case, but for most inputs
// it does O(n) work beyond a sort on constant length integer keys.
// The depth is O(log^2 n) assuming the sort is within that bound.
// Each round doubles the substring lengths that have been sorted but
// drops any strings that are already sorted (i.e. have no equal
// strings for the current length).  For most inputs, most strings
// (suffixes) drop out early.
// This code only works on up to 2^32 - 12 characters,
// Chars treated as unsigned, and 0 (null character) not allowed.
// **************************************************************
template <typename char_range>
auto suffix_array(const char_range& S) {
  using index = unsigned int;
  using ulong = unsigned long;
  struct seg { index start; index end; };
  index n = S.size();
  int granularity = 100; // only effects performance, and not by much

  // pack 12 chars starting at each index, and index into two unsigned longs and sort.
  auto s = [&] (index i) -> index {return (i < n) ? (unsigned char) S[i] : 0;};
  auto Clx = parlay::delayed_tabulate(n, [&] (ulong i) {
    ulong high = 0, low = 0;
    for (int j=0; j < 8; j++) high = (high << 8) + s(i+j);
    for (int j=0; j < 4; j++) low = (low << 8) + s(8+i+j);
    return std::pair{high, (low << 32) + i};});
  auto Cl = parlay::sort(Clx);

  // Unpack sorted pairs placing index in sorted, and marking where
  // values changed in flags.  Now correctly sorted up to 12 characters.
  auto sorted = parlay::sequence<index>::uninitialized(n);
  auto ranks = parlay::sequence<index>::uninitialized(n);
  auto flags = parlay::sequence<bool>::uninitialized(n);
  parlay::parallel_for(0, n, [&] (long j) {
    auto [high,low] = Cl[j];
    sorted[j] = low & ((1ul << 32) - 1);
    flags[j] = ((j == 0) || (high != Cl[j-1].first) ||
                (low >> 32) != (Cl[j-1].second >> 32));});

  // Given flags indicating segment boundaries within a segment,
  // splits into new segments.  See below for definition of segments.
  // Also updates the ranks
  auto segs_from_flags = [&] (parlay::sequence<bool>& flags, index seg_start) {
    auto offsets = parlay::pack_index(flags);
    auto segs = parlay::tabulate(offsets.size(), [&] (long j) {
      index start = seg_start + offsets[j];
      index end = seg_start + ((j == offsets.size()-1) ? flags.size() : offsets[j+1]);
      parlay::parallel_for(start, end, [&] (index i) {ranks[sorted[i]] = start;}, granularity);
      return seg{start, end};}, granularity);
    return filter(segs, [] (seg s) {return (s.end - s.start) > 1;});};

  auto segments = segs_from_flags(flags, 0);
  index offset = 12;

  // This loop has the following invariants at the start of each iteration
  //   the suffixes are sorted up to the first "offset" characters
  //   "sorted" maintains the sorted indices of suffixes so far (eventually the result)
  //   "segs" maintains segments of contiguous regions of the sorted order that are not
  //      yet sorted (i.e. first offset characters are equal).  Each is represented
  //      as a start and end index for the segment.
  //   "ranks" maintains the ranks of each suffix based on the sort so far. All ranks
  //      for suffixes in the same segment are the same (rank of first in segment).
  // Each iteration doubles the size of offset, so will complete in at most log n rounds.
  // In practice few segments survive for multiple rounds.
  while (segments.size() > 0) {
    // for suffixes in live segments grabs ranks from offset away and sorts within segment.
    // mark in flags where the keys now differ.
    auto flags = parlay::map(segments, [&] (seg segment) {
      index s = segment.start;
      index l = segment.end - segment.start;
      auto p = parlay::tabulate(l, [&] (long i) {
        index k = sorted[s + i];
        return std::pair{(k + offset >= n) ? 0 : ranks[k + offset], k};}, granularity);
      parlay::sort_inplace(p);
      parlay::sequence<bool> flags(l);
      parlay::parallel_for(0, l, [&] (long i) {
        sorted[s + i] = p[i].second;
        flags[i] = (i == 0) || p[i].first != p[i-1].first;}, granularity);
      return flags;}, 1);

    // break segments into smaller ones and throw away ones of length 1
    segments = parlay::flatten(parlay::tabulate(segments.size(), [&] (long i) {
      return segs_from_flags(flags[i], segments[i].start);}, 1));

    offset = 2 * offset;
  }
  return sorted;
}
