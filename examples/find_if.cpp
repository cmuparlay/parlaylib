#include <parlay/primitives.h>
#include <parlay/random.h>

// **************************************************************
// Finds location i of first element  that satisfies a predicate
// Importantly it runs with only O(i) work, while using O(log^2 i) span.
// Based on doubling search.
// This is available in parlaylib.
// **************************************************************

template <typename Range, typename UnaryPredicate>
long find_if_(const Range& r, UnaryPredicate p) {
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
	         return p(r[i + start]) ? i + start : n;}), parlay::minm<long>());
    if (loc < n) return loc;
    start += len;
    len *= 2;
  }
  return n;
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: find_if <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random r;

    // generate n random numbers from 0 .. n-1
    auto vals = parlay::tabulate(n, [&] (long i) {return (r[i] % n);});

    long result = find_if_(vals, [] (long i) {return i == 277;});

    if (result == n) std::cout << "not found" << std::endl;
    else std::cout << "found at location " << result << std::endl;
  }
}
