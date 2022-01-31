#include <parlay/primitives.h>
#include <parlay/io.h>
#include <parlay/internal/block_delayed.h>
namespace delayed = parlay::block_delayed;

using ulong = unsigned long;
const ulong p = 1045678717;
static const ulong x = 500000000;
auto multm = parlay::make_monoid([] (ulong a, ulong b) {return (a*b)%p;}, 1ul);
auto addm = parlay::make_monoid([] (ulong a, ulong b) {return (a+b)%p;}, 0ul);

template <typename T>
struct rabin_karp {
private:
  parlay::sequence<ulong> hashes;
  parlay::sequence<T> str;
  ulong total;
  long n;

  auto powers_of_x(long n) {
    auto xs = parlay::delayed_tabulate(n, [&] (long i) {return x;});
    auto [powers, total] = parlay::scan(xs, multm);
    return powers;
  }

  template <typename Range>
  auto polynomial_terms(const Range& s) {
    //return delayed::zip_with(parlay::iota(n), powers_of_x(n),
    // [&] (long i, ulong xi) {
    //return ((ulong) s[i]) * xi;});
    auto foo = powers_of_x(n);
    return parlay::tabulate(s.size(), [&] (long i) {
    					return (((ulong) s[i]) * foo[i])%p;});
  }

public:
  template <typename Range>
  rabin_karp(const Range& s)
    : n(std::size(s)), str(parlay::to_sequence(s)) {
    auto [c, sum] = parlay::scan(polynomial_terms(s), addm);
    hashes = c; // delayed::force(c);
    total = sum;
  }

  template <typename Range>
  long find(const Range& s) {
    long m = s.size();
    ulong hash = parlay::reduce(polynomial_terms(s), addm);
    auto check = [&] (long i, ulong xi) {
		   if (i > n - m) return n;
		   ulong hash_end = (i == n - m) ? total: hashes[i+m];
		   if (((hash*xi) + hashes[i])%p == hash_end) return i;
		   return n;
		 };
    auto a = delayed::zip_with(parlay::iota(n), powers_of_x(n), check);
    return delayed::reduce(a, parlay::minm<long>());
    //auto cc = powers_of_x(n);
    //auto a = parlay::tabulate(n, [&] (long i) {return check(i, cc[i]);});
    //return parlay::reduce(a, parlay::minm<long>());
  }
};

// **************************************************************
// Driver code
// **************************************************************
using charseq = parlay::sequence<char>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: rabin_karp <search_string> <filename>";
  if (argc != 3) std::cout << usage << std::endl;
  else {
    charseq search_str = parlay::to_chars(argv[1]);
    charseq str = parlay::chars_from_file(argv[2]);
    rabin_karp<char> rk(str);
    long loc = rk.find(search_str);
    std::cout << "location found : " << loc << std::endl;
  }
}
