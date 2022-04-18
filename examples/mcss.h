#include <algorithm>
#include <array>
#include <limits>

#include <parlay/primitives.h>
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
