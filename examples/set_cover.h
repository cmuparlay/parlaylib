#include <limits>
#include <functional>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

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

template <typename idx>
struct set_cover {
  // Type definitions
  using sets = parlay::sequence<parlay::sequence<idx>>;
  using set_ids = parlay::sequence<idx>;
  using elements = parlay::sequence<std::atomic<idx>>;
  using buckets = parlay::sequence<parlay::sequence<idx>>;
  using bool_seq = parlay::sequence<bool>;
  static constexpr idx covered = -1;
  static constexpr idx not_covered = std::numeric_limits<idx>::max();

  // **************************************************************
  // The workhorse.
  // MANIS (MAximally Nearly Independent Set), gives a subset of the sets that nearly do
  // not share any elements.   In particular each element of the subset with
  // size m shares at most 2 * epsilon * m with others.
  // **************************************************************
  static set_ids manis(const set_ids& Si, sets& S, elements& E, bool_seq& in_result,
                       float low, float high) {
    auto reserve = [&] (idx i) {
      idx sid = Si[i];
      if (S[sid].size() < high) return done;

      // keep just elements that are not covered
      S[sid] = parlay::filter(S[sid], [&] (idx e) {return E[e] > 0;});

      // if enough elements remain then reserve them with priority i
      if (S[sid].size() >= high) {
        for (idx e : S[sid]) parlay::write_min(&E[e], i, std::less<idx>());
        return try_commit;
      } else return done;
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

    speculative_for(0, (int) Si.size(), reserve, commit, Si.size()/4);

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
  static set_ids run(const sets& Sin, long num_elements, double epsilon=.1) {
    sets S = Sin;  // copy since will be mutated
    long num_sets = S.size();

    double log1e = 1.0/log(1.0 + epsilon);
    auto bucket_from_size = [=] (long n) { return floor(log(n) * log1e);};

    double max_size = parlay::reduce(parlay::map(S, parlay::size_of()),
                                     parlay::maximum<idx>());
    int num_buckets = 1 + bucket_from_size(max_size);

    auto bucket_sets_by_size = [&] (const set_ids& Si) {
      auto B = parlay::tabulate(Si.size(), [&] (idx i) -> std::pair<int,idx> {
        return std::pair(bucket_from_size(S[Si[i]].size()), Si[i]);});
      return parlay::group_by_index(B, num_buckets);
    };

    // initial bucketing
    set_ids ids = parlay::filter(parlay::iota<idx>(num_sets), [&] (idx i) {
      return S[i].size() > 0;});
    buckets Bs = bucket_sets_by_size(ids);
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
};
