#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds a maximal matching for a graph.
// Uses "deterministic reservations", see:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun.
// Will generate same matching as a greedy sequential matching.
// **************************************************************

template <typename edges>
parlay::sequence<long> maximal_matching(edges const &E, long n) {
  parlay::sequence<reservation<long>> R(n);
  parlay::sequence<bool> matched(n, false);

  // tries to reserve both endpoints with edge i if neither are matched
  auto reserve = [&] (long i) {
    auto u = E[i].first;
    auto v = E[i].second;
    if (matched[u] || matched[v] || (u == v)) return done;
    R[u].reserve(i);
    R[v].reserve(i);
    return try_commit;
  };

  // checks if successfully reserved both endpoints
  // if so mark endpoints as matched and return true
  // otherwise if succeeded on one, reset it
  auto commit = [&] (long i) {
    auto u = E[i].first;
    auto v = E[i].second;
    if (R[v].check(i)) {
      R[v].reset(); // so only one endpoint has edge id in it
      if (R[u].check(i)) {
        matched[u] = matched[v] = true;
        return true;
      }
    } else if (R[u].check(i)) R[u].reset();
    return false;
  };

  // loops over edged in parallel blocks
  speculative_for<long>(0, E.size(), reserve, commit, n/10);

  // returns the edges that successfully committed (their reservation remains in R[v]).
  return parlay::pack(parlay::delayed::map(R, [&] (auto& r) {return r.get();}),
                      parlay::tabulate(n, [&] (long i) {return R[i].reserved();}));
}
