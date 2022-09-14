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
  if (n == 2) {
     out[0] = in[0] + in[s];
     out[1] = in[0] - in[s];
  } else {
    parlay::par_do_if(n >= 256,
		      [&] {fft_recursive(n/2, 2*s, in, out, w);},
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
  auto w = powers(nth_root, in.size()/2);
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
// A more cache friendly version.
// It works across columns, then rows with transpose to convert.
// Input and output in "column major" order.
// See:
//   David Baily
//   FFTs in External or Hierarchical Memory
//   Journal of Supercomputing, 1990
// **************************************************************

// loop-based fft (assumes A is in bit-reveresed order)
template <typename T>
auto fft_base(parlay::sequence<T>& A,
	      const parlay::sequence<T>& w) {
  long n = A.size();
  int is = n/2;
  for (int s = 1; s < n; s = s*2, is = is/2) 
    for (int i = 0; i < n; i += 2*s)
      for (int j = 0; j < s; j++) {
	T p = A[i + j];
	T q = A[i + j + s] * w[j*is]; 
	A[i + j] = p + q;
	A[i + j + s] = p - q;
      }
}

auto bit_reverse(long n) {
  long bits = std::log2(n);
  return parlay::tabulate(n, [&] (long i) {
      int j = i & 1; 
      for (int k=1; k < bits; k++) {j <<= 1; i >>= 1; j |= i & 1;}
      return j;});
}

template <typename T>
auto fft_transpose(const parlay::sequence<parlay::sequence<T>>& cols, T nth_root) {
  long num_cols = cols.size();
  long num_rows = cols[0].size();
    
  // process columns
  auto br = bit_reverse(num_rows);
  parlay::sequence<T> wc = powers(std::pow(nth_root, num_cols), num_rows);
  auto columns = parlay::tabulate(num_cols, [&] (long i) {
      auto c = parlay::tabulate(num_rows, [&] (long j) {
	  return cols[i][br[j]];});
      fft_base(c, wc);
      auto w = powers(std::pow(nth_root, i), num_rows);
      // rotate to avoid stride problems with set-associative cache 
      return parlay::tabulate(num_rows, [&] (long j) {
	  int k = (num_rows + j - i) & (num_rows - 1);
	  return c[k]*w[k];});});

  // transpose and process rows
  br = bit_reverse(num_cols);
  auto wr = powers(std::pow(nth_root, num_rows), num_cols);
  return parlay::tabulate(num_rows, [&] (long j) {
      auto row = parlay::tabulate(num_cols, [&] (long i) {
	  return columns[i][i+j & (num_rows - 1)];}); // transpose
      auto r = parlay::tabulate(num_cols, [&] (long i) {
	  return row[br[i]];});
      fft_base(r, wr);
      return r;});
}

template <typename Matrix>
auto complex_fft_transpose(const Matrix& a) {
  long n = a.size() * a[0].size();
  double pi = std::acos(-1);
  std::complex<double> i{0.0,1.0};
  auto nth_root = std::exp(-(2 * pi)/n * i);
  return fft_transpose(a, nth_root);
}
