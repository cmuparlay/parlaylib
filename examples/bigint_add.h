#include <algorithm>
#include <array>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Adds two "big integers" of arbitrary precision.  Supports signed
// integers using two's complement.  Each integer is represented as a
// sequence of unsigned ints (most significant last).  The most
// significant bit of the most significant element indicates the sign.
// Uses a scan for the carry propagation.
// **************************************************************
using bigint = parlay::sequence<unsigned int>;

// Templatizing allows using delayed sequences (or std:vectors).
// Can take an extra one to add in.  Useful for subtraction.
template <typename Bigint1, typename Bigint2>
bigint add(const Bigint1& a, const Bigint2& b, bool extra_one=false) {
  using uint = unsigned int;
  using ulong = unsigned long;
  long na = a.size();  long nb = b.size();
  // flip order if a is smaller than b
  if (na < nb) return add(b, a, extra_one);
  if (nb == 0) return parlay::to_sequence(a);
  enum carry : char { no, yes, propagate};
  bool a_sign = a[na - 1] >> 31;
  bool b_sign = b[nb - 1] >> 31;
  ulong mask = (1l << 32) - 1;

  // used to extend b if it is shorter than a
  uint pad = b_sign ? mask : 0;
  auto B = [&] (long i) {return (i < nb) ? b[i] : pad;};

  // check which digits will carry or propagate
  auto c = parlay::tabulate(na, [&] (long i) {
    ulong s = a[i] + static_cast<ulong>(B(i));
    s += (i == 0 && extra_one);
    return (s >> 32) ? yes : (s == mask ? propagate : no);});

  // use scan to do the propagation
  auto f = [] (carry a, carry b) {return (b == propagate) ? a : b;};
  parlay::scan_inplace(c, parlay::binary_op(f, no));

  // Add last digit of a, b and the carry bit to get the last digit.
  uint last_digit = a[na-1] + B(na-1) + (c[na-1] == yes);
  // If signs of a and b are the same and differnt from last_digit,
  // it overflowed and we need to extend by an additional digit.
  bool add_digit = ((a_sign == b_sign) && ((last_digit >> 31) != a_sign));

  // final result -- add together digits of a, b and carry bit
  return parlay::tabulate(na + add_digit, [&] (long i) -> uint {
    return (i == na
            ? (a_sign ? mask : 1u)
            : (a[i] + B(i) + (c[i] == yes)));});
}

template <typename Bigint1, typename Bigint2>
bigint subtract(const Bigint1& a, const Bigint2& b) {
  // effectively negate b and add
  return add(a, parlay::delayed_map(b, [] (auto bv) { return ~bv; }), true);
}
