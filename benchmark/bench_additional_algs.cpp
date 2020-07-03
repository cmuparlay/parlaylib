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

#include "util.h"

// ------------------------- Benchmarks -------------------------------

static void bench_integer_sort(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto r = parlay::make_slice(v);
  for (auto _ : state) {
    parlay::integer_sort(r, [](auto x) { return x; }, 32);
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, N) BENCHMARK(bench_ ## NAME)->UseRealTime()->Unit(benchmark::kMillisecond)->Arg(N);

BENCH(integer_sort, 100000000);

