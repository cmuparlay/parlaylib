#include <algorithm>
#include <array>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "bigint_add.h"

// **************************************************************
// Karatsuba's algorithm for multiplying big integers
// Assumes integers are unsigned
// **************************************************************

// sequential n^2 version for small numbers
template <typename Bigint1, typename Bigint2>
bigint small_multiply(const Bigint1& a, const Bigint2& b) {
  long na = a.size();  long nb = b.size();
  if (na < nb) return small_multiply(b, a);
  unsigned long int mask = (1l << 32) - 1;

  bigint result(na + nb, 0);

  // caculate the result digits one by one propagating carry
  for (long i = 0; i < na + nb -1; i++) {
    unsigned long int accum = result[i];
    for (long j = std::max(0l, i-nb+1); j < std::min(i + 1, nb); j++)
      accum += a[j]*b[i-j];
    result[i] = accum & mask;
    result[i+1] = (accum >> 32); // carry propagate
  }
  return result;
}

template <typename Bigint>
auto shift(const Bigint& a, int n) {
  // use delayed version to avoid materializing result
  return parlay::delayed_tabulate(a.size() + n, [&,n] (long i) {
    return (i < n) ? 0 : a[i-n];});
}

template <typename Bigint1, typename Bigint2>
bigint karatsuba(const Bigint1& a, const Bigint2& b) {
  long na = a.size();  long nb = b.size();
  if (na < nb) return karatsuba(b,a);
  if (nb < 512) return small_multiply(a,b);
  long nhalf = nb/2;
  auto low_a = a.cut(0, nhalf);  auto high_a = a.cut(nhalf, na);
  auto low_b = b.cut(0, nhalf);  auto high_b = b.cut(nhalf, nb);
  bigint z0, z1, z2;
  parlay::par_do3([&] {z0 = karatsuba(low_a, low_b);},
                  [&] {z1 = karatsuba(add(low_a, high_a), add(low_b, high_b));},
                  [&] {z2 = karatsuba(high_a, high_b);});

  auto mid = subtract(z1, add(z0, z2));
  return add(shift(z2, 2 * nhalf), add(shift(mid, nhalf), z0));
}
