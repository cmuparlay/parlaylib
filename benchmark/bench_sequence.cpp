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

static void bench_grow_int64(benchmark::State& state) {
  parlay::sequence<int64_t> s;
  for (auto _ : state) {
    state.PauseTiming();
    s = parlay::sequence<int64_t>(10000000);
    state.ResumeTiming();
    s.reserve(s.capacity() + 1);    // Trigger grow
  }
}

// No annotation needed since this one should be detectable
struct Relocatable {
  std::unique_ptr<int> x;
  Relocatable() = default;
  Relocatable(int x_) : x(std::make_unique<int>(x_)) { }
};

#if defined(PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE)
namespace parlay {
template<>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(Relocatable);
}
#endif

static_assert(parlay::is_trivially_relocatable_v<Relocatable>);

struct NotRelocatable {
  std::unique_ptr<int> x;
  NotRelocatable() = default;
  NotRelocatable(int x_) : x(std::make_unique<int>(x_)) { }
  NotRelocatable(NotRelocatable&& other) noexcept : x(std::move(other.x)) { }
  ~NotRelocatable() { }
};
static_assert(!parlay::is_trivially_relocatable_v<NotRelocatable>);

static void bench_grow_relocatable(benchmark::State& state) {
  parlay::sequence<Relocatable> s;
  for (auto _ : state) {
    state.PauseTiming();
    s = parlay::sequence<Relocatable>(10000000);
    state.ResumeTiming();
    s.reserve(s.capacity() + 1);    // Trigger grow
  }
}

static void bench_grow_nonrelocatable(benchmark::State& state) {
  parlay::sequence<NotRelocatable> s;
  for (auto _ : state) {
    state.PauseTiming();
    s = parlay::sequence<NotRelocatable>(10000000);
    state.ResumeTiming();
    s.reserve(s.capacity() + 1);    // Trigger grow
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME) BENCHMARK(bench_ ## NAME)               \
                          ->UseRealTime()                   \
                          ->Unit(benchmark::kMillisecond);

BENCH(subscript);
BENCH(short_subscript);
BENCH(grow_int64);
BENCH(grow_relocatable);
BENCH(grow_nonrelocatable);
