#include <iostream>

#include <parlay/io.h>
#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Rabin-Karp string searching.
// Finds the first position of a search string of length m in a string
// of length n.  Generates a running hash such that the difference
// been two positions gives a hash for the string in between.  The
// search string can then be compared with the (n - m + 1) length m
// substrings of the input string in constant work per comparison.
// **************************************************************

// a finite field modulo a prime
// The prime needs to fit in 32 bits so multiplication into 64 bits
// does not overflow.
struct field {
  static constexpr unsigned int p = 1045678717;
  using l = unsigned long;
  unsigned int val;
  template <typename Int>
  field(Int i) : val(i) {}
  field() {}
  field operator+(field a) { return field(((l) val + (l) a.val)%p); }
  field operator*(field a) { return field(((l) val * (l) a.val)%p); }
  bool operator==(field a) { return val == a.val; }
};
auto multm = parlay::monoid([] (field a, field b) {return a*b;}, field(1));

// Works on any range types (e.g. std::string, parlay::sequence)
// Elements must be of integer type (e.g. char, int, unsigned char)
template <typename Range1, typename Range2>
long rabin_karp(const Range1& s, const Range2& str) {
  long n = s.size();
  long m = str.size();
  field x(500000000);

  // calculate hashes for prefixes of s
  auto xs = parlay::delayed_tabulate(n, [&] (long i) {
    return x;});
  auto [powers, total] = parlay::scan(xs, multm);
  auto terms = parlay::tabulate(n, [&, &powers = powers] (long i) {
    return field(s[i]) * powers[i];});
  auto [hashes, sum] = parlay::scan(terms);

  // calculate hash for str
  auto terms2 = parlay::delayed_tabulate(m, [&, &powers = powers] (long i) {
    return field(str[i]) * powers[i];});
  field hash = parlay::reduce(terms2);

  // find matches
  auto y = parlay::delayed_tabulate(n-m+1,
    [&, &powers = powers, &hashes = hashes, total = total] (long i) {
      field hash_end = (i == n - m) ? total: hashes[i+m];
      if (hash * powers[i] + hashes[i] == hash_end &&
          parlay::equal(str, s.cut(i,i+m))) // double check
        return i;
      return n; });
  return parlay::reduce(y, parlay::minm<long>());
}

// **************************************************************
// Driver code
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: rabin_karp <search_string> <filename>";
  if (argc != 3) std::cout << usage << std::endl;
  else {
    parlay::chars str = parlay::chars_from_file(argv[2]);
    parlay::chars search_str = parlay::to_chars(argv[1]);
    long loc = rabin_karp(str, search_str);
    if (loc < str.size())
      std::cout << "found at position: " << loc << std::endl;
    else std::cout << "not found" << std::endl;
  }
}
