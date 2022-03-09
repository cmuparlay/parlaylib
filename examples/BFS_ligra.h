#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/ligra_light.h"

// **************************************************************
// Parallel Breadth First Search (Using the Ligra interface)
// For each vertex returns the parent in the BFS tree.
// The start vertex points to itself, and any unvisited vertices have -1.
// The graph is a sequence of sequences of vertex ids, representing
// the outedges for each vertex.
// This version uses the ligra interface.  See: helper/ligra_light.h
// **************************************************************

using vertex = int;
using Graph = parlay::sequence<parlay::sequence<vertex>>;

auto BFS(vertex start, const Graph &G, const Graph& GT) {
  long n = G.size();
  auto parent = parlay::tabulate<std::atomic<vertex>>(n, [&] (size_t i) {
    return -1; });
  parent[start] = start;

  auto edge_f = [&] (vertex u, vertex v) -> bool {
    vertex expected = -1;
    return parent[v].compare_exchange_strong(expected, u);};
  auto cond_f = [&] (vertex v) { return parent[v] == -1;};
  auto frontier_map = ligra::edge_map(G, GT, edge_f, cond_f);

  auto frontier = ligra::vertex_subset(start);
  long visited = 0;

  while (frontier.size() > 0) {
    visited += frontier.size();
    frontier = frontier_map(frontier);
  }
  return std::pair{parlay::map(parent, [] (auto const &x) {
    return x.load();}), visited};
}
