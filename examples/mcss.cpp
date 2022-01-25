#include <parlay/primitives.h>
#include <parlay/random.h>

// **************************************************************
// Parallel Maximum Contiguous Subsequence Sum
// The algorithm uses reduce to maintain a 4-tuple for a range
//   1: the best solution in the range
//   2: the best solution starting at the begining
//   3: the best solution starting at the end
//   4: the sum of values in the range
// These can be combined using an associative function (f)
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
  return parlay::reduce(pre, parlay::make_monoid(f, identity))[0];
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: mcss <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random r;

    // generate n random numbers from -100 .. 100
    auto vals = parlay::tabulate(n, [&] (long i) -> int {
	          return (r[i] % 201) - 100;});
    auto result = mcss(vals);
    std::cout << "mcss = " << result << std::endl;
  }
}
