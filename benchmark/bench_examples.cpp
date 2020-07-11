// Benchmarks of example applications using parlay

#include <cctype>

#include <benchmark/benchmark.h>

#include <parlay/delayed_sequence.h>
#include <parlay/monoid.h>
#include <parlay/primitives.h>

// ------------------------- Word Count -----------------------------

template <class Seq>
std::tuple<size_t,size_t,size_t> wc(Seq const &s) {
  // Create a delayed sequence of pairs of integers:
  // the first is 1 if it is line break, 0 otherwise;
  // the second is 1 if the start of a word, 0 otherwise.
  auto f = parlay::delayed_seq<std::pair<size_t, size_t>>(s.size(), [&] (size_t i) {
    bool is_line_break = s[i] == '\n';
    bool word_start = ((i == 0 || std::isspace(s[i-1])) && !std::isspace(s[i]));
    return std::make_pair<size_t,size_t>(is_line_break, word_start);
  });

  // Reduce summing the pairs to get total line breaks and words.
  // This is faster than summing them separately since that would
  // require going over the input sequence twice.
  auto m = parlay::pair_monoid(parlay::addm<size_t>{}, parlay::addm<size_t>{});
  auto r = parlay::reduce(f, m);

  return std::make_tuple(r.first, r.second, s.size());
}

static void bench_wordcount(benchmark::State& state) {
  size_t n = state.range(0);
  std::string s(n, 'b');
  auto r = parlay::make_slice(s);
  for (auto _ : state) {
    wc(r);
  }
}

// ------------------------- Prime Sieve -----------------------------

// Recursively finds primes less than sqrt(n), then sieves out
// all their multiples, returning the primes less than n.
// Work is O(n log log n), Span is O(log n).
template <class Int>
parlay::sequence<Int> prime_sieve(Int n) {
  if (n < 2) return parlay::sequence<Int>();
  else {
    Int sqrt = std::sqrt(n);
    auto primes_sqrt = prime_sieve(sqrt);
    parlay::sequence<bool> flags(n+1, true);  // flags to mark the primes
    flags[0] = flags[1] = false;              // 0 and 1 are not prime
    parlay::parallel_for(0, primes_sqrt.size(), [&] (size_t i) {
      Int prime = primes_sqrt[i];
      parlay::parallel_for(2, n/prime + 1, [&] (size_t j) {
	      flags[prime * j] = false;
	    }, 1000);
    }, 1);
    return parlay::pack_index<Int>(flags);    // indices of the primes
  }
}

static void bench_prime_sieve(benchmark::State& state) {
  size_t n = state.range(0);

  for (auto _ : state) {
    prime_sieve(n);
  }
}

// ------------- Maximum Contiguous Subsequence Sum ------------------

// 10x improvement by using delayed sequence
template <class Seq>
typename Seq::value_type mcss(Seq const &A) {
  using T = typename Seq::value_type;
  auto f = [&] (auto a, auto b) {
    auto [aa, pa, sa, ta] = a;
    auto [ab, pb, sb, tb] = b;
    return std::make_tuple(
      std::max(aa,std::max(ab,sa+pb)),
      std::max(pa, ta+pb),
      std::max(sa + ab, sb),
      ta + tb
    );
  };
  auto S = parlay::delayed_seq<std::tuple<T,T,T,T>>(A.size(), [&] (size_t i) {
    return std::make_tuple(A[i],A[i],A[i],A[i]);
  });
  auto m = parlay::make_monoid(f, std::tuple<T,T,T,T>(0,0,0,0));
  auto r = parlay::reduce(S, m);
  return std::get<0>(r);
}

static void bench_mcss(benchmark::State& state) {
  size_t n = state.range(0);
  std::vector<long long> a(n);
  parlay::parallel_for(0, n, [&](auto i) {
    a[i] = (i % 2 == 0 ? -1 : 1) * i;
  });

  for (auto _ : state) {
    mcss(a);
  }
}

// ---------------------------- Integration ----------------------------

template <typename F>
double integrate(size_t num_samples, double start, double end, F f) {
   double delta = (end-start)/num_samples;
   auto samples = parlay::delayed_seq<double>(num_samples, [&] (size_t i) {
        return f(start + delta/2 + i * delta); });
   return delta * parlay::reduce(samples, parlay::addm<double>());
}

static void bench_integrate(benchmark::State& state) {
  size_t n = state.range(0);
  auto f = [&] (double q) -> double {
    return pow(q, 2);
  };
  double start = static_cast<double>(0);
  double end = static_cast<double>(1000);
  for (auto _ : state) {
    integrate(n, start, end, f);
  }
}


// ------------------------- Registration -------------------------------

#define BENCH(NAME, N) BENCHMARK(bench_ ## NAME)->UseRealTime()->Unit(benchmark::kMillisecond)->Arg(N);

BENCH(wordcount, 100000000);
BENCH(prime_sieve, 10000000);
BENCH(mcss, 100000000);
BENCH(integrate, 100000000);

