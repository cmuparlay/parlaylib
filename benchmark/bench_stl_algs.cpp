// Benchmarks for Parlay's implementation of
// common C++ standard library algorithms

#include <limits>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include <parlay/merge.h>
#include <parlay/monoid.h>
#include <parlay/parallel.h>
#include <parlay/sequence_ops.h>
#include <parlay/stlalgs.h>

// ------------------------- Utilities -------------------------------

// Thread local random number generation
thread_local std::default_random_engine generator;
thread_local std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
int rand() { return dist(generator); }

// Generate a random vector of length n consisting
// of uniformly random non-negative 32-bit integers
std::vector<int> random_vector(size_t n) {
  std::vector<int> a(n);
  parlay::parallel_for(0, n, [&](auto i) {
    a[i] = rand();
  });
  return a;
}

// ------------------------- Benchmarks -------------------------------

static void bench_adjacent_find(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::adjacent_find(v);
  }
}

static void bench_all_of(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::all_of(v, [](auto x) { return x != 0; });
  }
}

static void bench_any_of(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::any_of(v, [](auto x) { return x == 0; });
  }
}

static void bench_count(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::count(v, 0);
  }
}

static void bench_count_if(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::count_if(v, [](auto x) { return x != 0; });
  }
}

static void bench_equal(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto v2 = v;
  for (auto _ : state) {
    parlay::equal(v, v2);
  }
}

// exclusive_scan is just "scan" in parlay
static void bench_exclusive_scan(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto r = parlay::make_range(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::scan(r, parlay::addm<int>{});
  }
}

static void bench_find(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::find(v, 0);
  }
}

static void bench_find_end(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto v2 = random_vector(n/2);
  for (auto _ : state) {
    parlay::find_end(v, v2);
  }
}

static void bench_find_first_of(benchmark::State& state) {
  size_t n = (size_t)sqrt(state.range(0));
  auto v = random_vector(n);
  auto v2 = random_vector(n);
  for (auto _ : state) {
    parlay::find_first_of(v, v2, [](auto x, auto y) { return x == y; });
  }
}

static void bench_find_if(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::find_if(v, [](auto x) { return x == 0; });
  }
}

static void bench_find_if_not(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::find_if_not(v, [](auto x) { return x != 0; });
  }
}

static void bench_for_each(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::for_each(v, [](auto& x) { x += 1; });
  }
}

static void bench_is_partitioned(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  std::sort(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::is_partitioned(v, [&](auto x) { return x < v[n/2]; });
  }
}

static void bench_is_sorted(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  std::sort(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::is_sorted(v, std::less<int>{});
  }
}

static void bench_is_sorted_until(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  std::sort(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::is_sorted_until(v, std::less<int>{});
  }
}

static void bench_lexicographical_compare(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto v2 = v;
  for (auto _ : state) {
    parlay::lexicographical_compare(v, v2, std::less<int>{});
  }
}

static void bench_max_element(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::max_element(v, std::less<int>{});
  }
}

static void bench_merge(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto v2 = random_vector(n);
  auto r = parlay::make_range(std::begin(v), std::end(v));
  auto r2 = parlay::make_range(std::begin(v2), std::end(v2));
  for (auto _ : state) {
    parlay::merge(r, r2, std::less<int>{});
  }
}

static void bench_min_element(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::min_element(v, std::less<int>{});
  }
}

static void bench_minmax_element(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::minmax_element(v, std::less<int>{});
  }
}

static void bench_mismatch(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto v2 = v;
  for (auto _ : state) {
    parlay::mismatch(v, v2);
  }
}

static void bench_none_of(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::none_of(v, [](auto x) { return x == 0; });
  }
}

static void bench_reduce(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto r = parlay::make_range(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::reduce(r, parlay::addm<int>{});
  }
}

static void bench_remove_if(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  auto r = parlay::make_range(std::begin(v), std::end(v));
  for (auto _ : state) {
    parlay::remove_if(r, [](auto x) { return x % 2 == 0; } );
  }
}

static void bench_reverse(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::reverse(v);
  }
}

static void bench_rotate(benchmark::State& state) {
  size_t n = state.range(0);
  auto v = random_vector(n);
  for (auto _ : state) {
    parlay::rotate(v, n/2);
  }
}

static void bench_search(benchmark::State& state) {
  size_t n = (size_t)sqrt(state.range(0));
  auto v = random_vector(n);
  auto v2 = random_vector(n/2);
  for (auto _ : state) {
    parlay::search(v, v2);
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, N) BENCHMARK(bench_ ## NAME)->UseRealTime()->Unit(benchmark::kMillisecond)->Arg(N);

BENCH(adjacent_find, 100000000);
BENCH(all_of, 100000000);
BENCH(any_of, 100000000);
BENCH(count, 100000000);
BENCH(count_if, 100000000);
BENCH(equal, 100000000);
BENCH(exclusive_scan, 100000000);
BENCH(find, 100000000);
BENCH(find_end, 100000000);
BENCH(find_first_of, 100000000);
BENCH(find_if, 100000000);
BENCH(find_if_not, 100000000);
BENCH(for_each, 100000000);
BENCH(is_partitioned, 100000000);
BENCH(is_sorted, 100000000);
BENCH(is_sorted_until, 100000000);
BENCH(lexicographical_compare, 100000000);
BENCH(max_element, 100000000);
BENCH(merge, 100000000);
BENCH(min_element, 100000000);
BENCH(minmax_element, 100000000);
BENCH(mismatch, 100000000);
BENCH(none_of, 100000000);
BENCH(reduce, 100000000);
BENCH(remove_if, 100000000);
BENCH(reverse, 100000000);
BENCH(rotate, 100000000);
BENCH(search, 100000000);
