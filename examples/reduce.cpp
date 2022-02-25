#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>
#include <parlay/io.h>

// **************************************************************
// An implementation of reduce.
// Uses divide and conquer with a base case of block_size.
// Works on arbitrary "ranges" (e.g. sequences, delayed sequences,
//    std::vector, std::string, ..).
// **************************************************************

template<typename Range, typename BinaryOp>
auto reduce(const Range& A, const BinaryOp&& binop) {
  long n = A.size();
  using T = typename Range::value_type;
  long block_size = 100;
  if (n == 0) return binop.identity;
  if (n <= block_size) {
    T v = A[0];
    for (long i=1; i < n; i++)
      v = binop(v, A[i]);
    return v;
  }

  T L, R;
  parlay::par_do([&] {L = reduce(parlay::make_slice(A).cut(0,n/2), binop);},
		 [&] {R = reduce(parlay::make_slice(A).cut(n/2,n), binop);});
  return binop(L,R);
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: reduce <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::sequence ones(n,1);
    auto sum = ::reduce(ones, parlay::plus<int>());
    std::cout << "sum of ones = " << sum << std::endl;
  }
}
