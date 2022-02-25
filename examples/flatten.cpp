#include <iostream>
#include <string>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/range.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

// **************************************************************
// An implementation of flatten.
// Takes a nested sequence and flattens into a flat sequences.
// Basically as implemented in the library.
// **************************************************************

template <typename Range>
auto flatten(const Range& A) {
  using T = parlay::range_value_type_t<parlay::range_reference_type_t<Range>>;
  auto sizes = parlay::delayed_tabulate(A.size(), [&] (long i) {
    return A[i].size();});
  auto [offsets, m] = parlay::scan(sizes);
  auto r = parlay::sequence<T>::uninitialized(m);
  parlay::parallel_for(0, A.size(), [&, &offsets = offsets] (long i) {
    auto start = offsets[i];
    parlay::parallel_for(0, A[i].size(), [&] (long j) {
      parlay::assign_uninitialized(r[start+j], A[i][j]);
    }, 1000); });
  return r;
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: flatten <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    long sqrt = std::sqrt(n);
    parlay::sequence s(sqrt, parlay::sequence(sqrt, 1));
    auto result = ::flatten(s);

    std::cout << sqrt << "*" << sqrt << " = " << sqrt*sqrt
              <<" elements flattened" << std::endl;
  }
}
