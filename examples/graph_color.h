#include <unordered_set>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>
#include <parlay/random.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds an approximate minimum graph vertex coloring,
// i.e., no neighboring vertices can have the same color.
// Based on the greedy degree heuristic: coloring the vertices in
// order by degree (largest first).
// To respect serial ordering, uses "deterministic reservations".
// Returns the same coloring as the sequential algorithm.  See:
//    "Internally deterministic parallel algorithms can be fast"
//    Blelloch, Fineman, Gibbons, and Shun.
// Will generate same matching as a greedy sequential matching.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using graph = parlay::sequence<vertices>;

parlay::sequence<int> graph_coloring(graph const &G) {
  long n = G.size(); // number of vertices

  // rank vertices by degree, highest first
  auto ranks = parlay::rank(parlay::map(G, parlay::size_of()),
                            std::greater{});

  // inverse permutation of the rank
  parlay::sequence<vertex> ordering(n);
  parlay::parallel_for(0, n, [&] (vertex i) {ordering[ranks[i]] = i;});

  // -1 means unknown
  parlay::sequence<int> colors(n, -1);

  // checks all earlier neighbors by rank ordering have been colored
  auto is_ok = [&] (vertex i) {
    vertex u = ordering[i];
    for (vertex v : G[u])
      if (colors[v] == -1 && ranks[v] < i) return try_again;
    return try_commit;
  };

  // if so color this vertex
  auto succeeded = [&] (vertex i) {
    vertex u = ordering[i];
    auto ngh_colors = parlay::map(G[u], [&] (vertex v) {
      return colors[v];});
    std::sort(ngh_colors.begin(),ngh_colors.end());

    // find a color unused by any neighbor
    int color = -1;
    for (int c : ngh_colors) {
      if (c > color + 1) break; // gap in colors, use it
      color = c;
    }
    colors[u] = color + 1;
    return true;
  };

  // loops over the vertices
  speculative_for<vertex>(0, n, is_ok, succeeded);
  return colors;
}
