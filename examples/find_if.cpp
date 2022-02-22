#include <algorithm>
#include <iostream>
#include <string>
#include <random>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/random.h>

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

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: find_if <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<long> dis(0, n-1);

    // generate n random numbers from 0 .. n-1
    auto vals = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r); });

    long result = ::find_if(vals, [] (long i) { return i == 277; });

    if (result == n) std::cout << "not found" << std::endl;
    else std::cout << "found at location " << result << std::endl;
  }
}
