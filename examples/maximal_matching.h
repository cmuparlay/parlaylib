#include <utility>

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

using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;
using edgeid = long;
using res = reservation<edgeid>;

parlay::sequence<edgeid> maximal_matching(edges const &E, long n) {
  size_t m = E.size();

  parlay::sequence<res> R(n);
  parlay::sequence<bool> matched(n, false);

  // tries to reserve both endpoints with edge i if neither are matched
  auto reserve = [&] (edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
    if (matched[u] || matched[v] || (u == v)) return done;
    R[u].reserve(i);
    R[v].reserve(i);
    return try_commit;
  };

  // checks if successfully reserved both endpoints
  // if so mark endpoints as matched and return true
  // otherwise if succeeded on one, reset it
  auto commit = [&] (edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
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
  speculative_for<edgeid>(0, m, reserve, commit);

  // returns the edges that successfully committed (their reservation remains in R[v]).
  return parlay::pack(parlay::delayed::map(R, [&] (auto& r) {return r.get();}),
                      parlay::tabulate(n, [&] (size_t i) {return R[i].reserved();}));
}
