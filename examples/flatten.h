#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

// **************************************************************
// An implementation of flatten.
// Takes a nested sequence and flattens into a flat sequences.
// Basically as implemented in the library.
// **************************************************************

template <typename Range>
auto flatten(const Range& A) {
  using T = typename Range::value_type::value_type;
  auto sizes = parlay::delayed_map(A, parlay::size_of{});
  auto [offsets, m] = parlay::scan(sizes);
  auto r = parlay::sequence<T>::uninitialized(m);
  parlay::parallel_for(0, A.size(), [&, &offsets = offsets] (long i) {
    auto start = offsets[i];
    parlay::parallel_for(0, A[i].size(), [&] (long j) {
      parlay::assign_uninitialized(r[start+j], A[i][j]);
    }, 1000); });
  return r;
}
