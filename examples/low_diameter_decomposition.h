#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/delayed.h>
#include <parlay/sequence.h>

#include "helper/ligra_light.h"

// **************************************************************
// Low Diameter DeComposition
// For a given parameter Beta, clusters a graph so each cluster
// has diameter O((log n)/Beta) with high probability, and such
// that only a fraction Beta edges fall between clusters (whp).
// Requires the transpose graph (i.e the back edges).
// Returns for each vertex, the label of the center of its clusters.
// Algorithm from:
//   Gary L. Miller, Richard Peng, and Shen Chen Xu
//   Parallel graph decompositions using random shifts
//   SPAA 2013
// **************************************************************
template <typename vertex>
using graph = parlay::sequence<parlay::sequence<vertex>>;

template <typename vertex>
auto LDD(float beta, const graph<vertex>& G, const graph<vertex>& GT) {
  long n = G.size();
  parlay::random_generator g(0);
  std::exponential_distribution<float> exp(beta);

  // generate exp distribution and bucket based on it
  auto exps = parlay::tabulate(n, [&] (long i) {
    auto r = g[i]; return (int) std::floor(exp(r));});
  int max_e = parlay::reduce(exps, parlay::maximum<int>());
  auto buckets = parlay::group_by_index(parlay::delayed::tabulate(n, [&] (vertex i) {
    return std::pair(max_e - exps[i], i);}), max_e + 1);

  // -1 indicates unvisited
  auto labels = parlay::tabulate<std::atomic<vertex>>(n, [] (long i) {
    return -1;});

  auto edge_f = [&] (vertex u, vertex v) -> bool {
    vertex expected = -1;
    return labels[v].compare_exchange_strong(expected, labels[u]);};
  auto cond_f = [&] (vertex v) { return labels[v] == -1;};
  auto frontier_map = ligra::edge_map(G, GT, edge_f, cond_f);

  ligra::vertex_subset<vertex> frontier;
  for (int i = 0; i <= max_e; i++) {
    // add unvisited vertices from the next bucket to the frontier on each step
    frontier.add_vertices(parlay::filter(buckets[i], [&] (vertex v) {
      if (labels[v] != -1) return false;
      labels[v] = v;
      return true;}));
    frontier = frontier_map(frontier);
  }

  // pull out of atomic sequence
  return parlay::map(labels, [] (auto& l) {return l.load();});
}
