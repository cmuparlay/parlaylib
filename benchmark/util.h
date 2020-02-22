
#ifndef PARLAY_BENCHMARK_UTIL_H_
#define PARLAY_BENCHMARK_UTIL_H_

#include <parlay/random.h>
#include <parlay/utilities.h>
#include <parlay/stlalgs.h>

// Generate a random vector of length n consisting
// of random non-negative 64-bit integers.
parlay::sequence<long long> random_vector(size_t n) {
  parlay::random r(0);
  return parlay::sequence<long long>(n, [&] (auto i) {
      return r.ith_rand(i);
    });
}

// Generate a random sorted vector of length n consisting
// of random non-negative 64-bit integers.
parlay::sequence<long long> random_sorted_vector(size_t n) {
  auto a = random_vector(n);
  parlay::sort_inplace(a.slice(), [] (long long a, long long b) {
      return a < b;}); //std::less<long long>());
  return a;
}

// Generate a random vector of pairs of 64-bit integers.
parlay::sequence<std::pair<long long, long long>> random_pairs(size_t n) {
  parlay::random r(0);
  return parlay::sequence<std::pair<long long, long long>>(n, [&] (size_t i) {
      return std::make_pair(r.ith_rand(2*i), r.ith_rand(2*i + 1));
    });
}

#endif  // PARLAY_BENCHMARK_UTIL_H_
