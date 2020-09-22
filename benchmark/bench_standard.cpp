// A simple benchmark set with good performance coverage
// The main set used to evaluate performance enhancements
// to the library

#include <benchmark/benchmark.h>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include <parlay/internal/merge_sort.h>

template<typename T>
static void bench_map(benchmark::State& state) {
  size_t n = state.range(0);
  auto In = parlay::sequence<T>(n, 1);
  auto f = [&] (auto x) -> T { return x; };
  
  for (auto _ : state) {
    parlay::map(In, f);
  }
}

template<typename T>
static void bench_tabulate(benchmark::State& state) {
  size_t n = state.range(0);
  auto f = [] (size_t i) -> T { return i; };
  
  for (auto _ : state) {
    parlay::tabulate(n, f);
  }
}

template<typename T>
static void bench_reduce_add(benchmark::State& state) {
  size_t n = state.range(0);
  auto s = parlay::sequence<T>(n, 1);
  
  for (auto _ : state) {
    parlay::reduce(s);
  }
}

template<typename T>
static void bench_scan_add(benchmark::State& state) {
  size_t n = state.range(0);
  auto s = parlay::sequence<T>(n, 1);
  
  for (auto _ : state) {
    parlay::scan(s);
  }
}

template<typename T>
static void bench_pack(benchmark::State& state) {
  size_t n = state.range(0);
  auto flags = parlay::tabulate(n, [] (size_t i) -> bool {return i%2;});
  auto In = parlay::tabulate(n, [] (size_t i) -> T {return i;});
  
  for (auto _ : state) {
    parlay::pack(In, flags);
  }
}

template<typename T>
static void bench_gather(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return i;});
  auto idx = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) -> T {
    // prefetching helps significantly
    __builtin_prefetch (&in[idx[i+4]], 0, 1);
    return in[idx[i]];
  };
  
  for (auto _ : state) {
    if (n > 4) {
      parlay::tabulate(n-4, f);
    }
  }
}

template<typename T>
static void bench_scatter(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  parlay::sequence<T> out(n, 0);
  auto idx = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
    // prefetching makes little if any difference
    //__builtin_prefetch (&out[idx[i+4]], 1, 1);
      out[idx[i]] = i;
  };
      
  for (auto _ : state) {
    if (n > 4) {
      parlay::parallel_for(0, n-4, f);
    }
  }
}

template<typename T>
static void bench_write_add(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  
  std::atomic<T>* out = (std::atomic<T>*)::operator new(n * sizeof(std::atomic<T>));
  parlay::parallel_for(0, n, [&](size_t i) {
    new (std::addressof(out[i])) std::atomic<T>(0);
  });
  
  auto idx = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
    // putting write prefetch in slows it down
    //__builtin_prefetch (&out[idx[i+4]], 0, 1);
    //__sync_fetch_and_add(&out[idx[i]],1);};
    parlay::write_add(&out[idx[i]],1);
  };
    
  for (auto _ : state) {
    if (n > 4) {
      parlay::parallel_for(0, n-4, f);
    }
  }
  
  ::operator delete(out);
}

template<typename T>
static void bench_write_min(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  std::atomic<T>* out = (std::atomic<T>*)::operator new(n * sizeof(std::atomic<T>));
  parlay::parallel_for(0, n, [&](size_t i) {
    new (std::addressof(out[i])) std::atomic<T>(n);
  });
  
  auto idx = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
    // putting write prefetch in slows it down
    //__builtin_prefetch (&out[idx[i+4]], 1, 1);
    parlay::write_min(&out[idx[i]], (T) i, std::less<T>());
  };
  
  for (auto _ : state) {
    if (n > 4) {
      parlay::parallel_for(0, n-4, f);
    }
  }
  
  ::operator delete(out);
}

template<typename T>
static void bench_count_sort(benchmark::State& state) {
  size_t n = state.range(0);
  size_t bits = state.range(1);
  parlay::random r(0);
  size_t num_buckets = (1<<bits);
  size_t mask = num_buckets - 1;
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i);});
  parlay::sequence<T> out(n);
  auto f = [&] (size_t i) {return in[i] & mask;};
  auto keys = parlay::delayed_seq<unsigned char>(n, f);
  
  for (auto _ : state) {
    parlay::internal::count_sort<std::true_type, std::false_type>(
      parlay::make_slice(in),
      parlay::make_slice(out),
      parlay::make_slice(keys.begin(),keys.end()),
      num_buckets
    );
  }
}

template<typename T>
static void bench_random_shuffle(benchmark::State& state) {
  size_t n = state.range(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return i;});
  
  for (auto _ : state) {
    parlay::random_shuffle(in, n);
  }
}

template<typename T>
static void bench_histogram(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  parlay::sequence<T> out;
  
  for (auto _ : state) {
    out = parlay::histogram(in, (T) n);
  }
}

template<typename T>
static void bench_histogram_same(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::sequence<T> in(n, (T) 10311);
  parlay::sequence<T> out;
 
  for (auto _ : state) {
    out = parlay::histogram(in, (T) n);
  }
}

template<typename T>
static void bench_histogram_few(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%256;});
  parlay::sequence<T> out;
  
  for (auto _ : state) {
    out = parlay::histogram(in, (T) 256);
  }
}

template<typename T>
static void bench_integer_sort_pair(benchmark::State& state) {
  size_t n = state.range(0);
  using par = std::pair<T,T>;
  parlay::random r(0);
  size_t bits = sizeof(T)*8;
  auto S = parlay::tabulate(n, [&] (size_t i) -> par {
				 return par(r.ith_rand(i),i);});
  parlay::sequence<par> R;
  auto first = [] (par a) {return a.first;};
  
  for (auto _ : state) {
    R = parlay::internal::integer_sort(parlay::make_slice(S), first, bits);
  }
}

template<typename T>
static void bench_integer_sort(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  size_t bits = sizeof(T)*8;
  auto S = parlay::tabulate(n, [&] (size_t i) -> T {
				 return r.ith_rand(i);});
  auto identity = [] (T a) {return a;};
  parlay::sequence<T> R;
  
  for (auto _ : state) {
    R = parlay::internal::integer_sort(parlay::make_slice(S), identity, bits);
  }
}

template<typename T>
static void bench_integer_sort_128(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  size_t bits = parlay::log2_up(n);
  auto S = parlay::tabulate(n, [&] (size_t i) -> __int128 {
      return r.ith_rand(2*i) + (((__int128) r.ith_rand(2*i+1)) << 64) ;});
  auto identity = [] (auto a) {return a;};
  parlay::sequence<__int128> out;
  
  for (auto _ : state) {
    out = parlay::internal::integer_sort(parlay::make_slice(S),identity,bits);
  }
}

template<typename T>
static void bench_sample_sort(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  parlay::sequence<T> out;
  
  for (auto _ : state) {
    out = parlay::internal::sample_sort(parlay::make_slice(in), std::less<T>());
  }
}

template<typename T>
static void bench_merge(benchmark::State& state) {
  size_t n = state.range(0);
  auto in1 = parlay::tabulate(n/2, [&] (size_t i) -> T {return 2*i;});
  auto in2 = parlay::tabulate(n-n/2, [&] (size_t i) -> T {return 2*i+1;});
  parlay::sequence<T> out;
  
  for (auto _ : state) {
    out = parlay::merge(in1, in2, std::less<T>());
  }
}

template<typename T>
static void bench_merge_sort(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) -> T {return r.ith_rand(i)%n;});
  
  for (auto _ : state) {
    parlay::internal::merge_sort_inplace(make_slice(in), std::less<T>());
  }
}

template<typename T>
static void bench_split3(benchmark::State& state) {
  size_t n = state.range(0);
  auto flags = parlay::tabulate(n, [] (size_t i) -> unsigned char {return i%3;});
  auto In = parlay::tabulate(n, [] (size_t i) -> T {return i;});
  parlay::sequence<T> Out(n, 0);
  
  for (auto _ : state) {
    parlay::internal::split_three(make_slice(In), make_slice(Out), flags);
  }
}

template<typename T>
static void bench_quicksort(benchmark::State& state) {
  size_t n = state.range(0);
  parlay::random r(0);
  auto in = parlay::tabulate(n, [&] (size_t i) {return r.ith_rand(i)%n;});

  for (auto _ : state) {
    parlay::internal::p_quicksort_inplace(make_slice(in), std::less<T>());
  }
}

template<typename T>
static void bench_collect_reduce(benchmark::State& state) {
  size_t n = state.range(0);
  using par = std::pair<T,T>;
  parlay::random r(0);
  size_t num_buckets = (1<<8);
  auto S = parlay::tabulate(n, [&] (size_t i) -> par {
      return par(r.ith_rand(i) % num_buckets, 1);});
  auto get_key = [&] (const auto& a) {return a.first;};
  auto get_val = [&] (const auto& a) {return a.first;};
  
  for (auto _ : state) {
    parlay::internal::collect_reduce(S, get_key, get_val, parlay::addm<T>(), num_buckets);
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, T, args...) BENCHMARK_TEMPLATE(bench_ ## NAME, T)               \
                          ->UseRealTime()                                           \
                          ->Unit(benchmark::kMillisecond)                           \
                          ->Args({args});

BENCH(map, long, 100000000);
BENCH(tabulate, long, 100000000);
BENCH(reduce_add, long, 100000000);
BENCH(pack, long, 100000000);
BENCH(gather, long, 100000000);
BENCH(scatter, long, 100000000);
BENCH(write_add, long, 100000000);
BENCH(write_min, long, 100000000);
BENCH(count_sort, long, 100000000, 8);
BENCH(random_shuffle, long, 100000000);
BENCH(histogram, unsigned int, 100000000);
BENCH(histogram_same, unsigned int, 100000000);
BENCH(histogram_few, unsigned int, 100000000);
BENCH(integer_sort_pair, unsigned int, 100000000);
BENCH(integer_sort, unsigned int, 100000000);
BENCH(integer_sort_128, void, 100000000);
BENCH(sample_sort, long, 100000000);
BENCH(sample_sort, unsigned int, 100000000);
BENCH(sample_sort, __int128, 100000000);
BENCH(merge, long, 100000000);
BENCH(scatter, int, 100000000);
BENCH(merge_sort, long, 100000000);
BENCH(count_sort, long, 100000000, 2);
BENCH(split3, long, 100000000);
BENCH(quicksort, long, 100000000);
BENCH(collect_reduce, unsigned int, 100000000);
