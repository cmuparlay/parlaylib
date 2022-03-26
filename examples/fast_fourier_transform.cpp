#include <iostream>
#include <string>
#include <random>
#include <cmath>
#include <complex>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "fast_fourier_transform.h"

// **************************************************************
// Driver code
// **************************************************************
using complex = std::complex<double>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: fast_fourier_transform <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    int lg = std::floor(std::log2(n));
    n = 1 << lg;
    std::cout << n << std::endl;
    parlay::random_generator gen(0);
    std::uniform_real_distribution<double> dis(0.0,1.0);

    auto points = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return complex{dis(r), dis(r)};});

    parlay::sequence<complex> results;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      results = complex_fft(points);
      t.next("fast_fourier_transform");
    }

    std::cout << "first ten points " << std::endl;
    for (long i=0; i < std::min(10l,n); i++) 
      std::cout << results[i] << std::endl;
  }
}
