#include <parlay/primitives.h>

// **************************************************************
// Rabin-Karp string searching.
// Finds all position of a search string of length m in a string
// of length n.  Generates a running hash such that the difference
// been two positions gives a hash for the string in between.  The
// search string can then be compared with the (n - m + 1) length m
// substrings of the input string in constant work per comparison.
// **************************************************************

// a finite field modulo a prime
// The prime needs to fit in 32 bits so multiplication into 64 bits
// does not overflow.
struct field {
  static constexpr unsigned long p = 0x7fffffff; // Mersenne prime (2^31 - 1)
  unsigned int val;
  template <typename Int>
  field(Int i) : val(i) {}
  field() {}
  field operator+(field a) const {
    unsigned long x = (unsigned long) val + a.val;
    return field((x & p) + (x >> 31));} // fast mod p
  field operator*(field a) const {
    unsigned long x = (unsigned long) val * a.val;
    unsigned long y = (x & p) + (x >> 31);
    return field((y & p) + (y >> 31));} // fast mod p
  bool operator==(field a) const { return val == a.val; }
};

// Works on any range types (e.g. std::string, parlay::sequence)
// Elements must be of integer type (e.g. char, int, unsigned char)
template <typename Range1, typename Range2>
auto rabin_karp(const Range1& s, const Range2& str) {
  long n = s.size();
  long m = str.size();
  field x(500000000);

  // powers of x
  auto xs = parlay::delayed_tabulate(n, [&] (long i) { return x;});
  auto [powers, total] = parlay::scan(xs, parlay::multiplies<field>{});

  // hashes for prefixes of s
  auto terms = parlay::delayed_tabulate(n, [&, &powers = powers] (long i) {
    return field(s[i]) * powers[i];});
  auto [hashes, sum] = parlay::scan(terms);

  // hash for str
  auto terms2 = parlay::delayed_tabulate(m, [&, &powers = powers] (long i) {
    return field(str[i]) * powers[i];});
  field hash = parlay::reduce(terms2);

  // find matches
  auto y = parlay::delayed_tabulate(n-m+1,
                                    [&, &powers = powers, &hashes = hashes, total = total] (long i) {
                                      field hash_end = (i == n - m) ? total: hashes[i+m];
                                      if (hash * powers[i] + hashes[i] == hash_end &&
                                          parlay::equal(str, s.cut(i,i+m))) // double check
                                        return true;
                                      return false; });
  return parlay::pack_index<long>(y);
}
