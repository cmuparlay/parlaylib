#include <iostream>
#include <parlay/primitives.h>
#include <parlay/internal/block_delayed.h>
namespace delayed = parlay::block_delayed;

// Level: Advanced (heavy use of delayed sequences).

// **************************************************************
// An implementation of filter with other primitives This makes use of
// delayed sequences and should be competitive with the built in
// filter.
// It accepts a delayed sequence as an argument
// ************************************************************** 

template<typename Range, typename UnaryPred>
auto filter_(const Range& A, const UnaryPred&& f) {
  long n = A.size();
  using T = typename Range::value_type;
  auto flags = parlay::delayed_map(A, [&] (const T& x) {
		 return (long) f(x);});
  auto [offsets, sum] = delayed::scan(flags, parlay::addm<long>());
  auto r = parlay::sequence<T>::uninitialized(sum);
  delayed::zip_apply(parlay::iota(n), offsets, [&] (long i, long offset) {
      if (flags[i]) r[offset] = A[i];});
  return r;
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: filter <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    auto result = filter_(parlay::iota(n+1), [] (long i) {return i%2 == 0;});
    std::cout << "number of even integers up to n: " << result.size() << std::endl;
  }
}
