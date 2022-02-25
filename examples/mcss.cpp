#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <random>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

// **************************************************************
// Parallel Maximum Contiguous Subsequence Sum
// The algorithm maintain for a contiguous range a 4-tuple consisting of:
//   1: the best solution in the range
//   2: the best solution starting at the begining of the range
//   3: the best solution starting at the end of the range
//   4: the sum of values in the range
// These can be combined using an associative function (f) with reduce.
// **************************************************************

auto mcss(parlay::sequence<int> const& A) {
  using quad = std::array<long,4>;
  auto f = [] (quad a, quad b) {
    return quad{std::max(std::max(a[0],b[0]),a[2]+b[1]),
                std::max(a[1],a[3]+b[1]),
                std::max(a[2]+b[3],b[2]),
                a[3]+b[3]};};
  long neginf = std::numeric_limits<long>::lowest();
  quad identity = {neginf, neginf, neginf, 0l};
  auto pre = parlay::delayed_tabulate(A.size(), [&] (long i) -> quad {
    return quad{A[i],A[i],A[i],A[i]};});
  return parlay::reduce(pre, parlay::binary_op(f, identity))[0];
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: mcss <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<int> dis(-100, 100);

    // generate n random numbers from -100 .. 100
    auto vals = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r);});

    auto result = mcss(vals);
    std::cout << "mcss = " << result << std::endl;
  }
}
