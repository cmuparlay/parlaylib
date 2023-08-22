#include <benchmark/benchmark.h>

#include <parlay/thread_specific.h>

#include <folly/ThreadLocal.h>

parlay::ThreadSpecific<int> parlay_counter;

class NewTag;
folly::ThreadLocal<int, NewTag> folly_counter;

static thread_local int cpp_counter;

static __thread int gnu_counter;

static void bench_simple_int(benchmark::State& state) {
  int counter = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(counter += 1);
  }
}

static void bench_cpp_thread_local(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(cpp_counter += 1);
  }
}

static void bench_gnu_thread_local(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(gnu_counter += 1);
  }
}

static void bench_parlay_ts(benchmark::State& state) {
  for (auto _ : state) {
    *parlay_counter += 1;
  }
}

static void bench_folly_ts(benchmark::State& state) {
  for (auto _ : state) {
    *folly_counter += 1;
  }
}

static const std::size_t n_threads = 1;

BENCHMARK(bench_simple_int)->Threads(n_threads)->UseRealTime();
BENCHMARK(bench_cpp_thread_local)->Threads(n_threads)->UseRealTime();
BENCHMARK(bench_gnu_thread_local)->Threads(n_threads)->UseRealTime();
BENCHMARK(bench_parlay_ts)->Threads(n_threads)->UseRealTime();
BENCHMARK(bench_folly_ts)->Threads(n_threads)->UseRealTime();
