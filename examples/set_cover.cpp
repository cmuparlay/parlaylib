#include <iostream>
#include <cassert>
#include <math.h>
#include <limits>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"

#include "helper/speculative_for.h"

// **************************************************************
// Set Cover
// Given a set of sets, returns a subset that covers all elements the original set covered.
// Approximates the smallest such subset.
// Uses basically the algorithm from:
//    Guy Blelloch, Richard Peng and Kanat Tangwongsan. 
//    Linear-Work Greedy Parallel Approximation Algorithms for Set Covering and Variants. 
//    Proc. ACM Symposium on Parallel Algorithms and Architectures (SPAA), May 2011
// For a parameter epsilon (passed as an argument), returns a cover within (1+ epsilon) * (ln n)
// of optimal in theory (n is the sum of the set sizes).
// Work is O((1/epsilon)^2 * n) in theory.
// Span is O((1/epsilon)^2 * \log^3 n) in theory.
// Both work and approximation ratio are very much better in practice.
// **************************************************************

// Type definitions
using idx = int;  // the index of an element or set
using sets = parlay::sequence<parlay::sequence<idx>>;
using set_ids = parlay::sequence<idx>;
using elements = parlay::sequence<std::atomic<idx>>;
using buckets = parlay::sequence<parlay::sequence<idx>>;
using bool_seq = parlay::sequence<bool>;
constexpr idx covered = -1;
constexpr idx not_covered = std::numeric_limits<idx>::max();

// **************************************************************
// The workhorse.
// MANIS (MAximally Nearly Independent Set), gives a subset of the sets that nearly do
// not share any elements.   In particular each element of the subset with
// size m shares at most 2 * epsilon * m with others.
// **************************************************************
set_ids manis(const set_ids& Si, sets& S, elements& E, bool_seq& in_result,
	   float low, float high) {
  auto reserve = [&] (idx i) {
    idx sid = Si[i];
    if (S[sid].size() < high) return false;
    
    // keep just elements that are not covered
    S[sid] = parlay::filter(S[sid], [&] (idx e) {return E[e] > 0;});

    // if enough elements remain then reserve them with priority i
    if (S[sid].size() >= high) {
      for (idx e : S[sid]) parlay::write_min(&E[e], i, std::less<idx>());
      return true;
    } else return false;
  };

  auto commit = [&] (idx i) {
    idx sid = Si[i];
    // check how many were successfully reserved
    long k = 0;
    for (idx e : S[sid]) if (E[e] == i) k++;

    // if enough are reserved, then add set i to the Set Cover and
    // mark elements as covered
    if (k >= low) {
      for (idx e : S[sid]) if (E[e] == i) E[e] = covered;
      return in_result[sid] = true;
    } else {
      for (idx e : S[sid]) if (E[e] == i) E[e] = not_covered;
      return false;
    }
  };

  speculative_for(0, (int) Si.size(), reserve, commit, 1);

  // return sets that were not accepted
  return parlay::filter(Si, [&] (idx i) {
      return !in_result[i] && S[i].size() > 0;});
}

// **************************************************************
// The main routine.
// does O(log_(1 + epsilon) max_size) rounds each calling manis on
// sets with size:
//     ceil(pow(1 + epsilon, i+1)) > size >= ceil(pow(1 + epsilon, i-1))
// Sets are maintained bucketed by size
// **************************************************************
set_ids set_cover(const sets& Sin, long num_elements, double epsilon=.1) {
  sets S = Sin; // copy since S will be mutated
  long num_sets = S.size();

  double log1e = 1.0/log(1.0 + epsilon);
  auto bucket_from_size = [=] (long n) { return floor(log(n) * log1e);};

  double max_size = parlay::reduce(parlay::map(S, [] (auto& s) {return s.size();}),
				   parlay::maximum<idx>());
  int num_buckets = 1 + bucket_from_size(max_size);

  auto bucket_sets_by_size = [&] (const set_ids& Si) {
    auto B = parlay::tabulate(Si.size(), [&] (idx i) -> std::pair<int,idx> {
	return std::pair(bucket_from_size(S[Si[i]].size()), Si[i]);});
    return parlay::group_by_index(B, num_buckets);
  };

  // initial bucketing
  buckets Bs = bucket_sets_by_size(parlay::tabulate(num_sets, [] (idx i) {return i;}));
  auto B = parlay::map(Bs, [] (set_ids& si) {return buckets(1,si);});

  // initial sequences for tracking element coverage, and if set is in result
  elements E = parlay::tabulate<std::atomic<idx>>(num_elements, [&] (long i) {
      return not_covered; });
  bool_seq in_result(num_sets);

  // loop over all buckets, largest size first
  for (int i = B.size()-1; i >= 0; i--) {
    int high_threshold = ceil(pow(1.0+epsilon,i));
    int low_threshold = ceil(pow(1.0+epsilon,i-1));

    // get sets of appropriate size
    set_ids current = flatten(std::move(B[i]));
    if (current.size() == 0) continue;

    // run manis on them, which returns sets that were not added
    set_ids remain = manis(current, S, E, in_result, low_threshold, high_threshold);

    // buckets the not accepted sets by size and add to B
    buckets bs = bucket_sets_by_size(remain);
    for (int i = 0; i < num_buckets; i++)
      if (bs[i].size() > 0) B[i].push_back(std::move(bs[i]));
  }
  return parlay::pack_index<idx>(in_result);
}

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate random sets
// Exponentially distributed sizes
// **************************************************************
sets generate_sets(long n) {
  parlay::random_generator gen;
  std::exponential_distribution<float> exp_dis(.5);
  std::uniform_int_distribution<idx> int_dis(0,n);

  return parlay::tabulate(n, [&] (idx i) {
      auto r = gen[i];
      int m = static_cast<int>(floor(10.*(pow(1.2,exp_dis(r))))) -5;
      auto elts = parlay::tabulate(m, [&] (long j) {
	  auto rr = r[j];
	  return int_dis(rr);}, 100);
      return parlay::remove_duplicates(elts);});
}

// Check answer is correct (i.e. covers all elements that could be)
bool check(const set_ids& SI, const sets& S, long num_elements) {
  bool_seq a(num_elements, true);

  // set all that could be covered by original sets to false
  parlay::parallel_for(0, S.size(), [&] (long i) {
      for (idx j : S[i]) a[j] = false;});

  // set all that are covered by SI back to true
  parlay::parallel_for(0, SI.size(), [&] (long i) {
      for (idx j : S[SI[i]]) a[j] = true;});

  // check that all are covered
  return (parlay::count(a, true) == num_elements);
}
    
int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    float epsilon = .05;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    sets S = generate_sets(n);
    set_ids r;
    parlay::internal::timer t;
    for (int i=0; i < 3; i++) {
      r = set_cover(S, n, epsilon);
      t.next("set cover");    }
    if (check(r, S, n)) std::cout << "all elements covered!" << std::endl;
    long total = parlay::reduce(parlay::map(S, [] (auto s) {return s.size();}));
    std::cout << "sum of set sizes = " << total << std::endl;
    std::cout << "set cover size = " << r.size() << std::endl;
  }
}
