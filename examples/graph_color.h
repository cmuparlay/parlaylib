#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds an approximate minimum graph vertex coloring,
// i.e., no neighboring vertices can have the same color.
// Based on the greedy degree heuristic: coloring the vertices in
// order by degree (largest first).
// To respect serial ordering, uses "deterministic reservations".
// Returns the same coloring as the sequential algorithm.  See:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun.
// Will generate same matching as a greedy sequential matching.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using graph = parlay::sequence<vertices>;

parlay::sequence<int> graph_coloring(graph const &G) {
  // sort vertices by reverse degree
  auto less = [] (auto& a, auto& b) {return a.size() > b.size();};
  auto Gs = parlay::sort(G, less);
  
  // -1 means unknown
  parlay::sequence<int> colors(G.size(), -1);

  // checks all earlier neighbors have been colored
  auto is_ok = [&] (vertex u) {
    for (vertex v : G[u])
      if (v < u && colors[v] == -1) return try_again;
    return try_commit;
  };

  // if so color this vertex
  auto succeeded = [&] (vertex u) {
    auto ngh_colors = parlay::delayed::map(G[u], [&] (vertex v) {
	return colors[v];});
    
    // find a color unused by any neighbor
    int color = -1;
    for (int c : parlay::sort(ngh_colors)) {
      if (c > color + 1) break; // gap in colors, use it
      color = c;
    }
    colors[u] = color + 1;
    return true;
  };

  // loops over the vertices
  speculative_for<vertex>(0, G.size(), is_ok, succeeded);
  return colors;
}
