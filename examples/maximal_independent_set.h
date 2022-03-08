#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds a Maximal Independent Set (MIS).
// Uses "deterministic reservations" to find the lexicographically
// first MIS -- i.e. the one found by the greedy sequential algorithm
// on the given order.   This is the algorithm from:
//   Blelloch, Fineman, and Shun.
//   Greedy Sequential Maximal Independent Set and Matching are Parallel on Average
// Input order should be randomized if not already so.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using Graph = parlay::sequence<vertices>;

parlay::sequence<bool> MIS(Graph const &G) {
  parlay::sequence<bool> in_set(G.size(), false);
  parlay::sequence<bool> decided(G.size(), false);

  // checks all earlier neighbors have been decided
  // and if so and none are in_set, then add to set
  auto check_if_ready = [&] (vertex u) {
    for (vertex v : G[u])
      if (v < u)
	if (!decided[v]) return try_again;
	else if (in_set[v]) return try_commit;
    in_set[u] = true;
    return try_commit;
  };

  // mark as decided
  auto commit = [&] (vertex u) { return decided[u] = true;};

  // loops over the vertices
  speculative_for<vertex>(0, G.size(), check_if_ready, commit);
  return in_set;
}
