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

// use 128 bit integers if available
#ifdef __SIZEOF_INT128__
using digit = unsigned long;
using double_digit = unsigned __int128;
#else
using digit = unsigned int;
using double_digit = unsigned long;
#endif

constexpr int digit_len = sizeof(digit)*8;
using bigint = parlay::sequence<digit>;

namespace delayed = parlay::delayed;

// Templatizing allows using delayed sequences (or std:vectors).
// Can take an extra one to add in.  Useful for subtraction.
template <typename Bigint1, typename Bigint2>
bigint add(const Bigint1& a, const Bigint2& b, bool extra_one=false) {
  long na = a.size();  long nb = b.size();
  // flip order if a is smaller than b
  if (na < nb) return add(b, a, extra_one);
  if (nb == 0) return parlay::to_sequence(a);
  enum carry : char { no = 0, yes = 1, propagate = 2};
  
  bool a_sign = a[na - 1] >> (digit_len - 1);
  bool b_sign = b[nb - 1] >> (digit_len - 1);
  double_digit mask = (static_cast<double_digit>(1) << digit_len) - 1;

  // used to extend b if it is shorter than a
  digit pad = b_sign ? mask : 0;
  auto B = [&] (long i) {return (i < nb) ? b[i] : pad;};

  bigint result;
  if (na <  65536) { // if not large do sequentially
    double_digit carry = extra_one;
    result = bigint::uninitialized(na);
    for (int i=0; i < na; i++) {
      double_digit s = a[i] + (b[i] + carry);
      result[i] = s & mask;
      carry = s >> digit_len;
    }
  } else { // do in parallel
    // check which digits will carry or propagate
    auto c = delayed::tabulate(na, [&] (long i) {
	double_digit s = a[i] + static_cast<double_digit>(B(i));
	s += (i == 0 && extra_one);
	return static_cast<carry>(2 * (s == mask) + (s >> digit_len));}); 

    // use scan to do the propagation
    auto f = [] (carry a, carry b) {return (b == propagate) ? a : b;};
    auto cc = delayed::scan(c, parlay::binary_op(f, propagate)).first;
    auto z = delayed::zip(cc, parlay::iota(na));
    result = delayed::to_sequence(delayed::map(z, [&] (auto p) {
    	auto [ci, i] = p;
    	return a[i] + B(i) + ci;}));
    //auto cc = parlay::scan(c, parlay::binary_op(f, propagate)).first;
    //result = parlay::tabulate(na, [&] (long i) {
    //	return a[i] + B(i) + cc[i];});
  }
  if ((a_sign == b_sign) && ((result[na-1] >> (digit_len - 1)) != a_sign))
    result.push_back(a_sign ? mask : 1u);
  return result;
}

template <typename Bigint1, typename Bigint2>
bigint subtract(const Bigint1& a, const Bigint2& b) {
  // effectively negate b and add
  return add(a, delayed::map(b, [] (auto bv) {return ~bv;}),
	     true);
}
