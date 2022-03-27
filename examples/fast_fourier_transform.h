#include <complex>
#include <cmath>

#include <parlay/sequence.h>
#include <parlay/primitives.h>

// **************************************************************
// Fast Fourier transform
// First is a general form taking a sequence of n elements from any
// field along with the n-th roots of unity for the field.
// + and * must be defined on the elements.
// It is then specialized to complex numbers.
// Although gets quite decent speedup relatively to equally simple
// sequential version, this code is clearly not optimized.
// Input must have a length that is a power of 2.
// **************************************************************

template <typename Range>
Range evens(const Range& a) {
  return parlay::tabulate(a.size()/2, [&] (long i) {return a[2*i];});}

template <typename Range>
Range odds(const Range& a) {
  return parlay::tabulate(a.size()/2, [&] (long i) {return a[2*i+1];});}

template <typename Range>
Range fft(const Range& a, const Range& w) {
  using field = typename Range::value_type;
  long n = a.size();
  if (n == 1) return a;
  auto wn = evens(w);
  parlay::sequence<field> re, ro;
  parlay::par_do([&] {re = fft(evens(a), wn);},
		 [&] {ro = fft(odds(a), wn);});
  return parlay::tabulate(a.size(), [&] (long i) {
      long j = (i < n/2) ? i : i - n/2;
      return re[j] + ro[j] * w[i];},1000);
}

using complex_seq = parlay::sequence<std::complex<double>>;

complex_seq complex_fft(const complex_seq& a) {
  long n = a.size();
  double pi = std::acos(-1);
  std::complex<double> i{0.0,1.0};
  auto w = parlay::tabulate(a.size(), [&] (long j) {
      return std::exp((2 * pi * j)/n * i);});
  return fft(a, w);
}

