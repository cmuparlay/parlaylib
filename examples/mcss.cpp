#include <iostream>
#include <parlay/primitives.h>
#include <parlay/random.h>

// **************************************************************
// Parallel Maximum Contiguous Subsequence Sum
// The algorithm uses reduce to maintain 4 values for a range
//   1: the best solution in the range
//   2: the best solution starting at the begining
//   3: the best solution starting at the end
//   4: the sum of values in the range
// These can be combined using reduce to get the result
// Extracing the first (index 0) gives the result
// **************************************************************

auto mcss(parlay::sequence<int> const& A) {
  using tu = std::array<long,4>;
  auto f = [] (tu a, tu b) {
    tu r = {std::max(std::max(a[0],b[0]),a[2]+b[1]),
	    std::max(a[1],a[3]+b[1]),
	    std::max(a[2]+b[3],b[2]),
	    a[3]+b[3]};
    return r;};
  long neginf = std::numeric_limits<long>::lowest();
  tu identity = {neginf, neginf, neginf, 0l};
  auto pre = parlay::delayed_tabulate(A.size(), [&] (long i) -> tu {
      tu x = {A[i],A[i],A[i],A[i]};
      return x;
    });
  return parlay::reduce(pre, parlay::make_monoid(f, identity))[0];
}

int main(int argc, char* argv[]) {
  long n;
  if (argc != 2)
    std::cout << "linefit <n>" << std::endl;
  else {
    // should catch invalid argument exception if not an integer
    n = std::stol(argv[1]);
    parlay::random r;

    // generate n random numbers from -100 .. 100
    auto vals = parlay::tabulate(n, [&] (long i) -> int {
	          return (r[i] % 201) - 100;});
    auto result = mcss(vals);
    std::cout << "mcss = " << result << std::endl;
  }
}
