// Benchmarks for Parlay's implementation of
// additional, nonstandard algorithms

#include <functional>
#include <limits>
#include <random>
#include <thread>
#include <vector>

#include <benchmark/benchmark.h>

#include <parlay/collect_reduce.h>
#include <parlay/integer_sort.h>
#include <parlay/random.h>

// ------------------------- Utilities -------------------------------

// Thread local random number generation
thread_local size_t rand_i = 0;
thread_local parlay::random rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
size_t my_rand() { return rng.ith_rand(rand_i++); }

// Generate a random vector of length n consisting
// of random non-negative 32-bit integers.
// taking them mod 2^32.
std::vector<int> random_vector(size_t n) {
  std::vector<int> a(n);
  parlay::parallel_for(0, n, [&](auto i) {
    a[i] = my_rand() % (1LL << 32);
  });
  return a;
}

// ------------------------- Benchmarks -------------------------------

static void bench_integer_sort(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto r = parlay::make_range(&v[0], &v[0] + v.size());
  for (auto _ : state) {
    parlay::integer_sort(r, [](auto x) { return x; }, 32);
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, N) BENCHMARK(bench_ ## NAME)->UseRealTime()->Unit(benchmark::kMillisecond)->Arg(N);

BENCH(integer_sort, 100000000);

