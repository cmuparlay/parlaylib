// Benchmarks of parlay's sequence

#include <cctype>

#include <benchmark/benchmark.h>

#include <parlay/sequence.h>

// Benchmark the subscript operator. We expect a difference in performance
// with sequence and short_sequence since short_sequence should have to
// perform an additional check.
static void bench_subscript(benchmark::State& state) {
  auto s = parlay::sequence<int>(1000000);
  for (auto _ : state) {
    for (size_t i = 0; i < s.size(); i++) {
      benchmark::DoNotOptimize(s[i]);
    }
  }
}

static void bench_short_subscript(benchmark::State& state) {
  auto s = parlay::short_sequence<int>(1000000);
  for (auto _ : state) {
    for (size_t i = 0; i < s.size(); i++) {
      benchmark::DoNotOptimize(s[i]);
    }
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME) BENCHMARK(bench_ ## NAME)               \
                          ->UseRealTime()                   \
                          ->Unit(benchmark::kMillisecond);

BENCH(subscript);
BENCH(short_subscript);
