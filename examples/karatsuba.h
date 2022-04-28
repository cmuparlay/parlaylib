#include <algorithm>
#include <array>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "bigint_add.h"

// **************************************************************
// Karatsuba's algorithm for multiplying big integers
// Assumes integers are non-negative.
// Uses big integer addition and subtraction from bigint_add.h
// **************************************************************

// sequential n^2 version for small numbers
template <typename Bigint>
bigint small_multiply(const Bigint& a, const Bigint& b);

// shift a left by n digits (i.e multiply a by d^n).
auto shift(const bigint& a, int n) {
  return parlay::delayed::tabulate(a.size() + n, [&,n] (long i) {
      return (i < n) ? 0 : a[i-n];});}

// borrowed from the wikipedia page
template <typename Bigint>
bigint karatsuba(const Bigint& a, const Bigint& b) {
  long na = a.size();  long nb = b.size();
  if (na < nb) return karatsuba(b,a);
  if (nb <= 128) return small_multiply(a,b);
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

// **************************************************************
// Helper functions
// **************************************************************
template <typename Bigint>
bigint small_multiply(const Bigint& a, const Bigint& b) {
  long na = a.size();  long nb = b.size();
  if (na < nb) return small_multiply(b, a);
  double_digit mask = (static_cast<double_digit>(1) << digit_len) - 1;
  
  // calculate the result digits one by one propagating carry
  bigint result(na + nb, 0);
  for (long i = 0; i < na + nb -1; i++) {
    double_digit accum = result[i];
    for (long j = std::max(0l, i-nb+1); j < std::min(i + 1, nb); j++)
      accum += a[j]*b[i-j];
    result[i] = accum & mask;
    result[i+1] = (accum >> digit_len); // carry propagate
  }
  return result;
}
