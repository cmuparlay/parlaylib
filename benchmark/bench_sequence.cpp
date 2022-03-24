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

static void bench_push_back_std_alloc(benchmark::State& state) {
  auto s = parlay::sequence<int, std::allocator<int>>{};
  for (auto _ : state) {
    s.push_back(0);
  }
}

static void bench_push_back_parlay_alloc(benchmark::State& state) {
  auto s = parlay::sequence<int>{};
  for (auto _ : state) {
    s.push_back(0);
  }
}

static void bench_vector_push_back_std_alloc(benchmark::State& state) {
  auto s = std::vector<int>{};
  for (auto _ : state) {
    s.push_back(0);
  }
}

static void bench_vector_push_back_parlay_alloc(benchmark::State& state) {
  auto s = std::vector<int, parlay::allocator<int>>{};
  for (auto _ : state) {
    s.push_back(0);
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME) BENCHMARK(bench_ ## NAME)               \
                          ->UseRealTime()                   \
                          ->Unit(benchmark::kNanosecond);

BENCH(subscript);
BENCH(short_subscript);

BENCH(push_back_std_alloc);
BENCH(push_back_parlay_alloc);

BENCH(vector_push_back_std_alloc);
BENCH(vector_push_back_parlay_alloc);
