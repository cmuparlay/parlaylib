#include <algorithm>

#include <parlay/primitives.h>

// **************************************************************
// Finds location i of first element  that satisfies a predicate
// Importantly it runs with only O(i) work, while using O(log^2 i) span.
// Based on doubling search.
// This is available in parlaylib.
// **************************************************************

template <typename Range, typename UnaryPredicate>
long find_if(Range&& r, UnaryPredicate&& p) {
  long n = r.size();
  long len = 1000;
  long i;
  for (i = 0; i < (std::min)(len, n); i++)
    if (p(r[i])) return i;
  if (i == n) return n;
  long start = len;
  len = 2 * len;
  while (start < n) {
    long end = (std::min)(n, start + len);
    long loc = parlay::reduce(parlay::delayed_tabulate(end-start, [&] (long i) {
      return p(r[i + start]) ? i + start : n;}), parlay::minimum<long>());
    if (loc < n) return loc;
    start += len;
    len *= 2;
  }
  return n;
}
