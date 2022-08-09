// Benchmarks for methods for parsing primitive types
// (e.g. ints, doubles) from sequence<char>s.

#include <cctype>
#include <cmath>

#include <iomanip>
#include <limits>
#include <sstream>

#include <benchmark/benchmark.h>

#include <parlay/io.h>


template<typename T>
T input();

template<> int input<int>() { return std::numeric_limits<int>::max() / 2; }
template<> long input<long>() { return std::numeric_limits<long>::max() / 2; }
template<> long long input<long long>() { return std::numeric_limits<long long>::max() / 2; }

template<> float input<float>() { return std::acos(-1.0f); }
template<> double input<double>() { return std::acos(-1.0); }
template<> long double input<long double>() { return std::acos((long double)-1.0); }

template<typename T>
parlay::chars sinput(size_t p = 15) {
  std::stringstream ss;
  ss << std::setprecision(p);
  ss << input<T>();
  return parlay::to_chars(ss.str());
}

template<typename T>
static void bench_stringstream(benchmark::State& state) {
  auto s = sinput<T>();
  T answer;
  for (auto _ : state) {
    std::stringstream ss;
    ss << s;
    T res;
    ss >> res;
    benchmark::DoNotOptimize(answer = res);
  }
}

template<typename T>
static void bench_stoi(benchmark::State& state) {
  auto s = sinput<int>();
  int answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stoi(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_stol(benchmark::State& state) {
  auto s = sinput<long>();
  long answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stol(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_stoll(benchmark::State& state) {
  auto s = sinput<long long>();
  long long answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stoll(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_stof(benchmark::State& state) {
  auto s = sinput<float>(8);
  float answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stof(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_stod(benchmark::State& state) {
  auto s = sinput<double>();
  double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stod(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_stold(benchmark::State& state) {
  auto s = sinput<long double>();
  long double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = std::stold(std::string(std::begin(s), std::end(s))));
  }
}

template<typename T>
static void bench_chars_to_int(benchmark::State& state) {
  auto s = sinput<int>();
  int answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_int(s));
  }
}

template<typename T>
static void bench_chars_to_long(benchmark::State& state) {
  auto s = sinput<long>();
  long answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_long(s));
  }
}

template<typename T>
static void bench_chars_to_long_long(benchmark::State& state) {
  auto s = sinput<long long>();
  long answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_long_long(s));
  }
}

template<typename T>
static void bench_chars_to_float_fastpath(benchmark::State& state) {
  auto s = sinput<float>(7);
  float answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_float(s));
  }
}

template<typename T>
static void bench_chars_to_double_fastpath(benchmark::State& state) {
  auto s = sinput<double>(15);
  double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_double(s));
  }
}

template<typename T>
static void bench_chars_to_long_double_fastpath(benchmark::State& state) {
  auto s = sinput<long double>(15);
  long double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_long_double(s));
  }
}

template<typename T>
static void bench_chars_to_float_slowpath(benchmark::State& state) {
  auto s = sinput<float>(8);
  float answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_float(s));
  }
}

template<typename T>
static void bench_chars_to_double_slowpath(benchmark::State& state) {
  auto s = sinput<double>(17);
  double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_double(s));
  }
}

template<typename T>
static void bench_chars_to_long_double_slowpath(benchmark::State& state) {
  auto s = sinput<long double>(17);
  long double answer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(answer = parlay::chars_to_long_double(s));
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, T, ...) BENCHMARK_TEMPLATE(bench_ ## NAME, T)                   \
                          ->UseRealTime()                                           \
                          ->Unit(benchmark::kNanosecond)                            \
                          ->Args({__VA_ARGS__});


BENCH(stringstream, int);
BENCH(stringstream, long);
BENCH(stringstream, long long);
BENCH(stringstream, float);
BENCH(stringstream, double);
BENCH(stringstream, long double);

BENCH(stoi, int);
BENCH(stol, long);
BENCH(stoll, long long);
BENCH(stof, float);
BENCH(stod, double);
BENCH(stold, long double);

BENCH(chars_to_int, int);
BENCH(chars_to_long, long);
BENCH(chars_to_long_long, long long);

BENCH(chars_to_float_fastpath, float);
BENCH(chars_to_double_fastpath, double);
BENCH(chars_to_long_double_fastpath, long double);

BENCH(chars_to_float_slowpath, float);
BENCH(chars_to_double_slowpath, double);
BENCH(chars_to_long_double_slowpath, long double);

