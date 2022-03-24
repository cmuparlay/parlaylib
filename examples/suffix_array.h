#include <math.h>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>
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
// This code only works on up to 2^32 - 48 characters
// **************************************************************
template <typename uint_range>
auto suffix_array(const uint_range& S) {
  using itype = typename uint_range::value_type;
  using index = unsigned int;
  using uint128 = unsigned __int128;
  struct seg { index start; index end; };
  size_t n = S.size();
  size_t pad = 48;
  parlay::internal::timer t;
  
  itype maxv = parlay::reduce(S, parlay::maximum<itype>());
  auto sa = parlay::histogram_by_index(S, ((index) maxv) + 1);
  auto ids = parlay::map(sa, [] (index i) {return (index) (i > 0);});
  index m = parlay::scan_inclusive_inplace(ids) + 1; 

  // pad the end of string with 0s
  auto s = parlay::tabulate(n + pad, [&] (size_t i) {
      return (i < n) ? ids[S[i]] : (index) 0;});
    
  // pack characters into 128-bit word, along with the location i
  // 96 bits for characters, and 32 for location
  double logm = log2((double) m);
  index nchars = floor(96.0/logm);
  std::cout << nchars << std::endl;

  auto Clx = parlay::delayed_tabulate(n, [&] (size_t i) {
      uint128 r = s[i];
      for (index j=1; j < nchars; j++) r = r*m + s[i+j];
      return (r << 32) + i;});
  t.next("head");
  
  // sort based on packed words
  auto Cl = parlay::sort(Clx, std::less<uint128>());
  t.next("sort");
  
  auto sorted = parlay::sequence<index>::uninitialized(n);
  auto flags = parlay::sequence<bool>::uninitialized(n);
  parlay::parallel_for(0, n, [&] (long j) {
      size_t mask = ((((size_t) 1) << 32) - 1);
      sorted[j] = Cl[j] & mask;
      flags[j] = (j == 0) || (Cl[j] >> 32) != (Cl[j-1] >> 32); });
  t.next("pass");
  Cl.clear();  
  t.next("clear");

  auto ranks = parlay::sequence<index>::uninitialized(n);
  auto segs_from_flags = [&] (parlay::sequence<bool>& flags, index seg_start) {
    auto offsets = parlay::pack_index(flags);
    auto segs = parlay::tabulate(offsets.size(), [&] (long j) {
	index start = seg_start + offsets[j];
	index end = seg_start + ((j == offsets.size()-1) ? flags.size() : offsets[j+1]);
	parlay::parallel_for(start, end, [&] (index i) {ranks[sorted[i]] = start;}, 100);
	return seg{start, end};}, 100);
    return filter(segs, [] (seg s) {return (s.end - s.start) > 1;});};

  auto segments = segs_from_flags(flags, 0);
  index offset = nchars;
  t.next("seg from flags");
  
  // The following loop has the following invariants at the start of each iteration
  //   the suffixes are sorted up the first "offset" characters
  //   "ranks" maintains the ranks of each suffix based on sort so far
  //   "sorted" maintains the sorted indices of suffixes so far (eventually the result)
  //   "segs" maintains segments of contiguous regions of the sorted order that are not
  //      yet sorted (i.e. first offset characters are equal).  Each is represented
  //      as a start and end index for the segment.
  // Each iteration doubles the size of offset, so will complete in at most log n rounds.
  // In practice few segments survive for multiple rounds.
  while (segments.size() > 0) {
    // for unsorted suffixes grab ranks from offset away and sort within segment
    // mark in flags where the keys now differ
    auto flags = parlay::map(segments, [&] (seg segment) {
	index s = segment.start;
	index l = segment.end - segment.start;
	auto p = parlay::tabulate(l, [&] (long i) {
	    index k = sorted[s + i];
	    return std::pair{(k + offset >= n) ? 0 : ranks[k + offset], k};}, 200);
	parlay::sort_inplace(p);
	parlay::sequence<bool> flags(l);
	parlay::parallel_for(0, l, [&] (long i) {
	    sorted[s + i] = p[i].second;
	    flags[i] = (i == 0) || p[i].first != p[i-1].first;});
	return flags; });
    t.next("mid");

    // break segments into smaler ones and throw away ones of length 1
    segments = parlay::flatten(parlay::tabulate(segments.size(), [&] (long i) {
	  return segs_from_flags(flags[i], segments[i].start);}));
    t.next("end");

    offset = 2 * offset;
  }
  return sorted;
}
