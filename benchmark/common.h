// Some common useful macros for benchmarking

#ifndef PARLAY_BENCHMARK_COMMON_H
#define PARLAY_BENCHMARK_COMMON_H

// Set the number of iterations for benchmarks to perform for
// their warmup (runs of the benchmark loop without timing)
// and the real measured iterations.
constexpr size_t BENCHMARK_WARMUP_ITERATIONS = 10;
constexpr size_t BENCHMARK_REAL_ITERATIONS   = 20;

using benchmark::Counter;

// Use this macro to avoid accidentally timing the destructors
// of the output produced by algorithms that return data
//
// The expression e is evaluated as if written in the context
// auto result_ = (e);
//
#define RUN_AND_CLEAR(e)      \
  {                           \
    auto result_ = (e);       \
    state.PauseTiming();      \
  }                           \
  state.ResumeTiming();

// Use this macro to copy y into x without measuring the cost
// of the copy in the benchmark
//
// The effect of this macro on the arguments x and is equivalent
// to the statement (x) = (y)
#define COPY_NO_TIME(x, y)    \
  state.PauseTiming();        \
  (x) = (y);                  \
  state.ResumeTiming();

// Report bandwidth and throughput statistics for the benchmark
//
// Arguments:
//  n:             The number of elements processed
//  bytes_read:    The number of bytes read per element processed
//  bytes_written: The number of bytes written per element processed
//
#define REPORT_STATS(n, bytes_read, bytes_written)                                                                                   \
  state.counters["       Bandwidth"] = Counter(state.iterations()*(n)*((bytes_read) + 0.7 * (bytes_written)), Counter::kIsRate);     \
  state.counters["    Elements/sec"] = Counter(state.iterations()*(n), Counter::kIsRate);                                            \
  state.counters["       Bytes/sec"] = Counter(state.iterations()*(n)*(sizeof(T)), Counter::kIsRate);

#define SETUP_CODE(setup_code_)                                                     \
  if (!warmup) state.PauseTiming();                                                 \
  setup_code_;                                                                      \
  if (!warmup) state.ResumeTiming();                                                \

#define BENCHMARK_LOOP(measure_loop_)                                               \
  {                                                                                 \
    bool warmup = true;                                                             \
    /* Run some warmup loops */                                                     \
    for (size_t i_ = 0; i_ < BENCHMARK_WARMUP_ITERATIONS; i_++) {                   \
      measure_loop_;                                                                \
    }                                                                               \
    warmup = false;                                                                 \
    while (state.KeepRunningBatch(BENCHMARK_REAL_ITERATIONS)) {                     \
      for (size_t i_ = 0; i_ < BENCHMARK_REAL_ITERATIONS; i_++) {                   \
        {                                                                           \
          measure_loop_;                                                            \
          state.PauseTiming();                                                      \
        }                                                                           \
        state.ResumeTiming();                                                       \
      }                                                                             \
    }                                                                               \
  }

// Register a templated benchmark with a given type parameter and input size
// Benchmarks will be measured in Milliseconds, using real time to ensure
// that measurements accuractly reflect the cost of parallel code
#define BENCH(NAME, T, args...) BENCHMARK_TEMPLATE(bench_ ## NAME, T)               \
                          ->UseRealTime()                                           \
                          ->MeasureProcessCPUTime()                                 \
                          ->Unit(benchmark::kMillisecond)                           \
                          ->Args({args});                                           \

#endif  // PARLAY_BENCHMARK_COMMON_H
