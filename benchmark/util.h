
#ifndef PARLAY_BENCHMARK_UTIL_H_
#define PARLAY_BENCHMARK_UTIL_H_

#include <parlay/random.h>
#include <parlay/utilities.h>

// Thread local random number generation
thread_local size_t rand_i = 0;
thread_local parlay::random rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
size_t my_rand() { return rng.ith_rand(rand_i++); }

// Generate a random vector of length n consisting
// of random non-negative 64-bit integers.
std::vector<long long> random_vector(size_t n) {
  std::vector<long long> a(n);
  parlay::parallel_for(size_t(0), n, [&](auto i) {
    a[i] = my_rand();
  });
  return a;
}

// Generate a random sorted vector of length n consisting
// of random non-negative 64-bit integers.
std::vector<long long> random_sorted_vector(size_t n) {
  auto a = random_vector(n);
  parlay::parallel_for(size_t(0), n, [&](auto i) {
    a[i] = a[i] % (std::numeric_limits<long long>::max() / n);
  });
  std::vector<long long> p(n);
  std::partial_sum(std::begin(a), std::end(a), std::begin(p));
  return p;
}

// Generate a random vector of pairs of 64-bit integers.
std::vector<std::pair<long long, long long>> random_pairs(size_t n) {
  auto x = random_vector(n), y = random_vector(n);
  std::vector<std::pair<long long, long long>> a(n);
  parlay::parallel_for(size_t(0), n, [&](auto i) {
    a[i] = std::make_pair(my_rand(), my_rand());
  });
  return a;
}

#endif  // PARLAY_BENCHMARK_UTIL_H_
