// Benchmarks of applications of block-iterable delayed sequences

#include <limits>
#include <optional>
#include <utility>

#include <benchmark/benchmark.h>

#include <parlay/delayed.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

// ======================================================================
//                             Tokens
// ======================================================================

template <typename F>
auto tokens_delayed(const parlay::sequence<char>& seq, F is_space) {
  size_t n = seq.size();
  auto A = seq.begin();  // Take a pointer to the buffer to avoid the overhead of SSO

  auto is_start = [&] (size_t i) { return ((i == 0) || is_space(A[i-1])) && !(is_space(A[i]));};
  auto is_end = [&] (size_t i) { return  ((i == n) || (is_space(A[i]))) && (i != 0) && !is_space(A[i-1]);};

  // associative combining function
  // first = # of starts, second = index of last start
  using ipair = std::pair<long,long>;
  auto f = [] (ipair a, ipair b) { return (b.first == 0) ? a : ipair(a.first+b.first,b.second);};

  auto in = parlay::delayed_tabulate(n+1, [&] (size_t i) -> ipair {
    return is_start(i) ? ipair(1,i) : ipair(0,0);});

  auto [offsets, sum] = parlay::delayed::scan(in, f, ipair(0,0));

  auto z = parlay::delayed::zip(offsets, parlay::iota(n+1));

  auto r = parlay::sequence<ipair>::uninitialized(sum.first);
  parlay::delayed::apply(z, [&] (auto&& x) {
    if (is_end(std::get<1>(x)))
      parlay::assign_uninitialized(r[std::get<0>(x).first], ipair(std::get<0>(x).second, std::get<1>(x)));
  });

  return r;
}


static void bench_tokens(benchmark::State& state) {
  size_t n = state.range(0);
  auto str = parlay::tabulate(n, [] (size_t i) -> char { return (i%8 == 0) ? ' ' : 'a';});
  auto is_space = [] (char c) {
    switch (c)  {
      case '\r': case '\t': case '\n': case ' ' : return true;
      default : return false;
    }
  };

  for (auto _ : state) {
    {
      auto t = tokens_delayed(str, is_space);
      benchmark::DoNotOptimize(t);
      state.PauseTiming();
    }
    state.ResumeTiming();
  }
}

// ======================================================================
//                             Primes
// ======================================================================

parlay::sequence<long> primes_delayed(long n) {
  if (n < 2) return parlay::sequence<long>();
  long sq = std::sqrt(n);
  auto sqprimes = primes_delayed(sq);
  parlay::sequence<bool> flags(n+1, true);
  auto sieves = parlay::map(sqprimes, [&] (long p) {
    return parlay::delayed_tabulate(n/p - 1, [=] (long m) {
      return (m+2) * p;});});
  auto s = parlay::delayed::flatten(sieves);
  parlay::delayed::apply(s, [&] (long j) {flags[j] = false;});
  flags[0] = flags[1] = false;
  return parlay::pack_index<long>(flags);
}

static void bench_primes(benchmark::State& state) {
  size_t n = state.range(0);

  for (auto _ : state) {
    {
      auto p = primes_delayed(n);
      benchmark::DoNotOptimize(p);
      state.PauseTiming();
    }
    state.ResumeTiming();
  }
}

// ======================================================================
//                             Bignum Add
// ======================================================================

// bignum represented as sequence of bytes, each with values in the range [0..128)
// i.e. think of as radix 128 integer representation
using digit = unsigned char;
using bignum = parlay::sequence<digit>;
constexpr digit BASE = 128;

auto big_add_delayed(bignum const& A, bignum const &B) {
  size_t n = A.size();
  auto sums = parlay::delayed_tabulate(n, [&] (size_t i) -> digit {
    return A[i] + B[i]; });
  auto f = [] (digit a, digit b) -> digit { // carry propagate
    return (b == BASE-1) ? a : b;};
  auto [carries, total] = parlay::delayed::scan(sums, f, digit(BASE-1));

  auto mr = parlay::delayed::zip_with([](digit carry, digit sum) -> digit {
    return ((carry >= BASE) + sum) % BASE;
  }, carries, sums);
  auto r = parlay::delayed::to_sequence(mr);

  return std::make_pair(std::move(r), (total >= BASE));
}

static void bench_bignum_add(benchmark::State& state) {
  size_t n = state.range(0);

  auto a = parlay::tabulate(n, [] (size_t i) -> digit { return i % 128; });
  auto b = parlay::tabulate(n, [] (size_t i) -> digit { return i % 128; });

  for (auto _ : state) {
    {
      auto [sums, carry] = big_add_delayed(a,b);
      benchmark::DoNotOptimize(sums);
      benchmark::DoNotOptimize(carry);
      state.PauseTiming();
    }
    state.ResumeTiming();
  }
}

// ======================================================================
//                             Best Cut
// ======================================================================

using index_t = int;

// Constant for switching to sequential versions
int minParallelSize = 1000;

struct range {
  float min;
  float max;
  range(float _min, float _max) : min(_min), max(_max) {}
  range() {}
};

struct event {
  float v;
  index_t p;
  event(float value, index_t index, bool type)
      : v(value), p((index << 1) + type) {}
  event() {}
};

#define START 0
#define IS_START(_event) (!(_event.p & 1))
#define END 1
#define IS_END(_event) ((_event.p & 1))

struct cutInfo {
  float cost;
  float cutOff;
  index_t numLeft;
  index_t numRight;
  cutInfo(float _cost, float _cutOff, index_t nl, index_t nr)
      : cost(_cost), cutOff(_cutOff), numLeft(nl), numRight(nr) {}
  cutInfo() {}
};

// sequential version of best cut
cutInfo bestCutSerial(parlay::sequence<event> const &E, range r, range r1, range r2) {
  double flt_max = std::numeric_limits<double>::max();
  index_t n = E.size();
  if (r.max - r.min == 0.0) return cutInfo(flt_max, r.min, n, n);
  float area = 2 * (r1.max-r1.min) * (r2.max-r2.min);
  float orthoPerimeter = 2 * ((r1.max-r1.min) + (r2.max-r2.min));

  // calculate cost of each possible split
  index_t inLeft = 0;
  index_t inRight = n/2;
  float minCost = flt_max;
  index_t k = 0;
  index_t rn = inLeft;
  index_t ln = inRight;
  for (index_t i=0; i <n; i++) {
    if (IS_END(E[i])) inRight--;
    float leftLength = E[i].v - r.min;
    float leftSurfaceArea = area + orthoPerimeter * leftLength;
    float rightLength = r.max - E[i].v;
    float rightSurfaceArea = area + orthoPerimeter * rightLength;
    float cost = (leftSurfaceArea * inLeft + rightSurfaceArea * inRight);
    if (cost < minCost) {
      rn = inRight;
      ln = inLeft;
      minCost = cost;
      k = i;
    }
    if (IS_START(E[i])) inLeft++;
  }
  return cutInfo(minCost, E[k].v, ln, rn);
}

// parallel version of best cut
cutInfo bestCut(parlay::sequence<event> const &E, range r, range r1, range r2) {
  index_t n = E.size();
  if (n < minParallelSize)
    return bestCutSerial(E, r, r1, r2);
  double flt_max = std::numeric_limits<double>::max();
  if (r.max - r.min == 0.0) return cutInfo(flt_max, r.min, n, n);

  // area of two orthogonal faces
  float orthogArea = 2 * ((r1.max-r1.min) * (r2.max-r2.min));

  // length of the perimeter of the orthogonal faces
  float orthoPerimeter = 2 * ((r1.max-r1.min) + (r2.max-r2.min));

  // count number that end before i
  auto is_end = parlay::delayed_tabulate(n, [&] (index_t i) -> index_t {return IS_END(E[i]);});
  auto end_counts = parlay::delayed::scan_inclusive(is_end);

  // calculate cost of each possible split location,
  // return tuple with cost, number of ends before the location, and the index
  using rtype = std::tuple<float,index_t,index_t>;

  auto func = [&] (index_t num_ends, index_t i) -> rtype {
    index_t num_ends_before = num_ends - IS_END(E[i]);
    index_t inLeft = i - num_ends_before; // number of points intersecting left
    index_t inRight = n/2 - num_ends;   // number of points intersecting right
    float leftLength = E[i].v - r.min;
    float leftSurfaceArea = orthogArea + orthoPerimeter * leftLength;
    float rightLength = r.max - E[i].v;
    float rightSurfaceArea = orthogArea + orthoPerimeter * rightLength;
    float cost = leftSurfaceArea * inLeft + rightSurfaceArea * inRight;
    return rtype(cost, num_ends_before, i);
  };

  auto costs = parlay::delayed::map(parlay::delayed::zip(end_counts, parlay::iota(n)),
        [&](auto&& x) { return std::apply(func, x); });

  // find minimum across all, returning the triple
  auto min_f = [&] (rtype a, rtype b) {return (std::get<0>(a) < std::get<0>(b)) ? a : b;};
  rtype identity(std::numeric_limits<float>::max(), 0, 0);
  auto [cost, num_ends_before, i] = parlay::delayed::reduce(costs, min_f, identity);

  index_t ln = i - num_ends_before;
  index_t rn = n/2 - (num_ends_before + IS_END(E[i]));
  return cutInfo(cost, E[i].v, ln, rn);
}

static void bench_bestcut(benchmark::State& state) {
  size_t n = state.range(0);

  parlay::sequence<event> events(2*n);
  parlay::parallel_for(0, n, [&](size_t i) {
    events[i*2].p = i;
    events[i*2+1].p = i;
    events[i*2].v = i;
    events[i*2+1].v = i + 1;
  });
  range r(0, 200000000.0f), r1(0, 200000000.0f), r2(0, 200000000.0f);

  for (auto _ : state) {
    auto c = bestCut(events, r, r1, r2);
    benchmark::DoNotOptimize(c);
  }
}

// ======================================================================
//                        Breadth-first Search
// ======================================================================

// Generates a random graph using the RMAT model
struct rMatGraph {

  rMatGraph(size_t n_, size_t seed, double a_, double b_, double c_) : n(n_), h(seed), a(a_), b(b_), c(c_) {}

  inline double rand_double(size_t i) {
    return static_cast<double>(parlay::hash64(i) & std::numeric_limits<int>::max())
           / (static_cast<double>(std::numeric_limits<int>::max())); }

  std::pair<int, int> f(size_t i, size_t randStart, size_t randStride) {
    if (i == 1) return {0, 0};
    else {
      auto x = f(i/2, randStart + randStride, randStride);
      double r = rand_double(randStart);
      if (r < a) return x;
      else if (r < a + b) return {x.first, x.second + i/2};
      else if (r < a + b + c) return {x.first + i/2, x.second};
      else return {x.first + i/2, x.second + i/2};
    }
  }

  auto edge(size_t i) {
    size_t randStart = parlay::hash64((2*i)*h);
    size_t randStride = parlay::hash64((2*i+1)*h);
    return f(n, randStart, randStride);
  }

  size_t n, h;
  double a, b, c;
};

// Generate a random graph of size n
auto make_graph(size_t n, size_t seed) {
  size_t m = 10 * n;
  size_t nn = (1ull << parlay::log2_up(n));

  // Parameters for generating RMat graph
  double a = 0.5, b = 0.1, c = b;
  rMatGraph rMat(nn,seed,a,b,c);

  // Make the graph symmetric by adding reverse edges
  auto E = parlay::tabulate(m, [&](size_t i) { return rMat.edge(i); });
  auto Erev = parlay::delayed_map(E, [&](auto&& e) { return std::make_pair(e.second, e.first); });
  auto Esym = parlay::remove_duplicates_ordered(parlay::append(E, Erev));

  // Convert edge array to adjacency list
  auto A = parlay::group_by_index(Esym, nn);

  return A;
}

using vertexId = int;
using adjList = parlay::sequence<parlay::sequence<vertexId>>;

parlay::sequence<vertexId> BFS(vertexId start, const adjList& G) {
  size_t n = G.size();
  auto parent = parlay::tabulate<std::atomic<vertexId>>(n, [&](size_t) { return -1; });
  parent[start] = start;
  parlay::sequence<vertexId> frontier(1,start);

  while (frontier.size() > 0) {

    // get out edges of the frontier and flatten
    auto nested_edges = parlay::map(frontier, [&] (vertexId u) {
      return parlay::delayed_tabulate(G[u].size(), [&, u] (size_t i) {
        return std::pair(u, G[u][i]);});});
    auto edges = parlay::delayed::flatten(nested_edges);

    // keep the v from (u,v) edges that succeed in setting the parent array at v to u
    auto edge_f = [&] (auto u_v) -> std::optional<vertexId> {
      vertexId expected = -1;
      auto [u, v] = u_v;
      if ((parent[v] == -1) && parent[v].compare_exchange_strong(expected, u)) {
        return std::make_optional(v);
      }
      else {
        return std::nullopt;
      }
    };

    frontier = parlay::delayed::to_sequence(parlay::delayed::filter_op(edges, edge_f));
  }

  // convert from atomic to regular sequence
  return parlay::map(parent, [] (auto const &x) -> vertexId { return x.load();});
}

static void bench_bfs(benchmark::State& state) {
  size_t n = state.range(0);
  size_t seed = parlay::hash64(1);
  auto G = make_graph(n, seed);

  for (auto _ : state) {
    {
      auto result = BFS(1, G);
      benchmark::DoNotOptimize(result);
      state.PauseTiming();
    }
    state.ResumeTiming();
  }
}

// ------------------------- Registration -------------------------------

#define BENCH(NAME, N) BENCHMARK(bench_ ## NAME)->UseRealTime()->Unit(benchmark::kMillisecond)->Arg(N)->Iterations(20);

BENCH(tokens, 500000000);
BENCH(primes, 100000000);
BENCH(bignum_add, 500000000);
BENCH(bestcut, 200000000);
BENCH(bfs, 10000000);
