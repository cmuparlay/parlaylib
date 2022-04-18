#include <atomic>
#include <utility>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/speculative_for.h"
// **************************************************************
// Parallel Random Permutation
// A parallel version of the Knuth (also Fisher-Yates) Shuffle
// From:
//    Sequential random permutation, list contraction and tree contraction are
//    highly parallel.
//    Shun, Gu, Blelloch, Fineman, and Gibbons
//    SODA 15
// Linear work and O(log n) depth.
// **************************************************************
template <typename Seq>
void random_shuffle(Seq& s) {
  long n = s.size();

  // initialize to an id out of range
  auto res = parlay::tabulate<std::atomic<long>>(n, [&] (long i) {
    return -1; });

  // every position i pick a "random" number in range [0,..,i].
  auto gen = parlay::random_generator(0);
  auto rand = parlay::tabulate(n, [&] (long i) {
    auto r = gen[i];
    return r() % (i + 1);});

  // reserve to see if earliest (min) to swap with rand[i]
  auto reserve = [&] (long i) {
    long j = rand[i];
    if (j == i || parlay::write_min(&res[j], i, std::less{}))
      return try_commit;
    return try_again;
  };

  // if earliest, then swap, else will try again
  auto commit = [&] (long i) {
    auto j = rand[i];
    if (j == i || res[j] == i) {
      std::swap(s[i],s[j]);
      res[i] = res[j] = n;
      return true;
    }
    return false;
  };

  speculative_for((long) 0, n, reserve, commit);
}
