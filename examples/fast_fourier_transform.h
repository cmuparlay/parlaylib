#include <complex>
#include <cmath>

#include <parlay/sequence.h>
#include <parlay/primitives.h>
#include <parlay/delayed.h>

// **************************************************************
// Fast Fourier transform
// Uses the Cooley-Tukey algorithm
// First is a general form taking a sequence of n elements from any
// field along with the n-th roots of unity for the field.
// + and * must be defined on the elements.
// It is then specialized to complex numbers.
// Includes a basic version, and a more cache friendly version.
// Input must have a length that is a power of 2.
// **************************************************************

template <typename T>
void fft_recursive(long n, long s, T const* in, T* out, T const* w) {
  if (n == 1)
    out[0] = in[0];
  else {
    parlay::par_do([&] {fft_recursive(n/2, 2*s, in, out, w);},
                   [&] {fft_recursive(n/2, 2*s, in+s, out+n/2, w);});
    parlay::parallel_for(0, n/2, [&] (long i) {
      T p = out[i];
      T q = out[i+n/2] * w[i * s];
      out[i] = p + q;
      out[i+n/2] = p - q;}, 1000);
  }
}

// generate first n powers of val
template <typename T>
auto powers(T val, long n) {
  return parlay::scan(parlay::delayed_tabulate(n, [&] (long i) {
    return val;}), parlay::multiplies<T>()).first;}

template <typename T>
auto fft(const parlay::sequence<T>& in, T nth_root) {
  // generate the n powers of nth_root of unity
  auto w = powers(nth_root, in.size());
  parlay::sequence<T> out(in.size());
  fft_recursive(in.size(), 1, in.data(), out.data(), w.data());
  return out;
}

using complex_seq = parlay::sequence<std::complex<double>>;

template <typename complex>
auto complex_fft(const parlay::sequence<complex>& a) {
  double pi = std::acos(-1);
  complex i{0.0,1.0};
  auto nth_root = std::exp(-(2 * pi)/a.size() * i);
  return fft(a, nth_root);
}

// **************************************************************
// A more cache friendly version that uses the fft above.
// It works across columns, then rows with transpose to convert.
// If kept in "column-major" order then could skip first and
// last transpose.
// See:
//   David Baily
//   FFTs in External or Hierarchical Memory
//   Journal of Supercomputing, 1990
// **************************************************************
template <typename T>
auto fft_transpose(const parlay::sequence<T>& in, T nth_root) {
  long lg = (std::log2(in.size())+1)/2;
  long num_columns = 1 << lg;
  long num_rows = in.size() / num_columns;

  auto columns = parlay::tabulate(num_columns, [&] (long i) {
    // transpose 1
    auto ai = parlay::tabulate(num_rows, [&] (long j) {
      return in[j * num_columns + i];});
    auto r = fft(ai, std::pow(nth_root, num_columns));
    auto w = powers(std::pow(nth_root, i), num_rows);
    return parlay::tabulate(num_rows, [&] (long j) {
      return r[j]*w[j];});}, 1);

  auto rows = parlay::tabulate(num_rows, [&] (long j) {
    // transpose 2
    auto aj = parlay::tabulate(num_columns, [&] (long i) {
      return columns[i][j];});
    return fft(aj, std::pow(nth_root, num_rows));}, 1);

  // transpose 3
  return parlay::flatten(parlay::tabulate(num_columns, [&] (long i) {
    return parlay::delayed::tabulate(num_rows, [&, i] (long j) {
      return rows[j][i];});}));
}

complex_seq complex_fft_transpose(const complex_seq& a) {
  double pi = std::acos(-1);
  std::complex<double> i{0.0,1.0};
  auto nth_root = std::exp(-(2 * pi)/a.size() * i);
  return fft_transpose(a, nth_root);
}
