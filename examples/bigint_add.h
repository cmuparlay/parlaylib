#include <algorithm>
#include <array>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Adds two "big integers"  of arbitrary precision.
// Supports signed integers using two's complement.
// Each integer is represented as a sequence of unsigned ints, with
// top bit indicating the sign.
// Uses a scan for the carry propagation.
// **************************************************************
using uint = unsigned int;
using ulong = unsigned long;
using bigint = parlay::sequence<uint>;

bigint add(const bigint& a, const bigint& b) {
  // ensure a is not smaller than b
  long na = a.size();  long nb = b.size();
  if (na < nb) return add(b,a);
  if (nb == 0) return a;
  enum carry : char { no, yes, propagate};
  bool a_sign = a[na - 1] >> 31;
  bool b_sign = b[nb - 1] >> 31;
  uint mask = (1l << 32) - 1;
  uint pad = b_sign ? mask : 0;

  // check which digits will carry or propagate
  auto c = parlay::tabulate(na, [&] (long i) {
      ulong s = a[i] + (ulong) (i < nb ? b[i] : pad);
      return (s >> 32) ? yes : (s == mask ? propagate : no);});

  // use scan to do the propagation
  auto f = [] (carry a, carry b) {return (b == propagate) ? a : b;};
  parlay::scan_inplace(c, parlay::binary_op(f, no));

  // check to see if need to add a digit
  uint last_digit = a[na-1] + (nb == na ? b[nb-1] : pad) + (c[na-1] == yes);
  bool add_digit = ((a_sign == b_sign) && ((last_digit << 31) != a_sign));

  // final result
  return parlay::tabulate(na + add_digit, [&] (long i) -> uint {
      return (i == na 
	      ? (a_sign ? mask : 1u)
	      : (a[i] + (i < nb ? b[i] : pad) + (c[i] == yes)));});
}
