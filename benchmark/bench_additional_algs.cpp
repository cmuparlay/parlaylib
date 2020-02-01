// Benchmarks for Parlay's implementation of
// additional, nonstandard algorithms

#include <functional>
#include <limits>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include <parlay/collect_reduce.h>
#include <parlay/integer_sort.h>

// ------------------------- Utilities -------------------------------

// Thread local random number generation
thread_local std::default_random_engine generator;
thread_local std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
int rand() { return dist(generator); }

// Generate a random vector of length n consisting
// of uniformly random non-negative 32-bit integers
std::vector<int> random_vector(size_t n) {
  std::vector<int> a(n);
  for (size_t i = 0; i < n; i++) {
    a[i] = rand();
  }
  return a;
}

std::vector<int> random_sorted_vector(size_t n) {
  std::vector<int> a(n);
  auto step = std::numeric_limits<int>::max() / n;
  for (size_t i = 1; i < n; i++) {
    a[i] = a[i-1] + (rand() % step);
  }
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

