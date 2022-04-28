#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/ligra_light.h"

// **************************************************************
// Parallel Breadth First Search (Using the Ligra interface)
// The graph is a sequence of sequences of vertex ids, representing
// the outedges for each vertex.
// Requires the transpose graph (i.e the back edges).
// Returns a sequence of sequences, with the ith element corresponding to
// all vertices at distance i (i.e. the i-th frontier during the search).
// This version uses the ligra interface.  See: helper/ligra_light.h
// Importantly it supports the forward-backwards optimization.  See:
//  Julian Shun, Guy E. Blelloch:
//  Ligra: a lightweight graph processing framework for shared memory.
//  PPOPP 2013:
// **************************************************************
template <typename vertex, typename graph>
auto BFS(vertex start, const graph& G, const graph& GT) {
  using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
  auto visited = parlay::tabulate<std::atomic<bool>>(G.size(), [&] (long i) {
    return (i==start) ? true : false; });

  auto edge_f = [&] (vertex u, vertex v) -> bool {
    bool expected = false;
    return visited[v].compare_exchange_strong(expected, true);};
  auto cond_f = [&] (vertex v) { return !visited[v];};
  auto frontier_map = ligra::edge_map(G, GT, edge_f, cond_f);

  auto frontier = ligra::vertex_subset(start);
  nested_seq frontiers;
  while (frontier.size() > 0) {
    frontiers.push_back(frontier.to_seq());
    frontier = frontier_map(frontier);
  }

  return frontiers;
}
