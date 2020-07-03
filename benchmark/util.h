
#ifndef PARLAY_BENCHMARK_UTIL_H_
#define PARLAY_BENCHMARK_UTIL_H_

#include <parlay/random.h>
#include <parlay/utilities.h>
#include <parlay/stlalgs.h>

// Generate a random vector of length n consisting
// of random non-negative 32-bit integers.
auto random_vector(size_t n) {
  parlay::random r(0);
  return parlay::sequence<long long>::from_function(n, [&] (auto i) {
    return r.ith_rand(i) % (1LL << 32);
  });
}

// Generate a random sorted vector of length n consisting
// of random non-negative 32-bit integers.
auto random_sorted_vector(size_t n) {
  auto a = random_vector(n);
  parlay::sort_inplace(make_slice(a), [] (auto a, auto b) {
      return a < b;});
  return a;
}

// Generate a random vector of pairs of 32-bit integers.
auto random_pairs(size_t n) {
  parlay::random r(0);
  return parlay::sequence<std::pair<long long, long long>>::from_function(n, [&] (size_t i) {
      return std::make_pair(r.ith_rand(2*i) % (1LL << 32),
                            r.ith_rand(2*i + 1) % (1LL << 32));
    });
}

#endif  // PARLAY_BENCHMARK_UTIL_H_
