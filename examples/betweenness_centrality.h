#include <atomic>
#include <optional>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "BFS_ligra.h"

// **************************************************************
// Betweenness centrality
// A parallel version of the algorithm from:
//   A Faster Algorithm for Betweenness Centrality
//   Ulrik Brandes
//   Journal of Mathematical Sociology, 2001
// This code just calculates from a single source.
// Can either be maped over all sources for exact BC, or mapped
// over a (random) sample of sources for approximate BC.
// **************************************************************
template <typename vertex, typename graph>
auto BC_single_source(vertex start, const graph &G, const graph &GT) {
  // get all levels of the BFS on G
  auto levels = BFS(start, G, GT);

  // label vertices with level and initialize sigma
  struct vtx {int level; float sigma; float delta;};
  parlay::sequence<vtx> V(G.size(), vtx{-1, 0.0, 0.0});
  V[start] = vtx{0, 1.0, 0.0};
  for_each(parlay::iota(levels.size()), [&] (long i) {
    for_each(levels[i], [&] (vertex v) {V[v].level = i;});});

  // forward pass over the levels to calculate sigma
  for (long i = 1; i < levels.size(); i++)
    for_each(levels[i], [&] (vertex v) {
      V[v].sigma = reduce(delayed::map(GT[v], [&] (vertex u) {
        return (V[u].level == i-1) ? V[u].sigma : 0.0;}));});

  // backward pass over the levels to calculate delta
  for (long i = levels.size()-2; i > 0; i--) {
    for_each(levels[i], [&] (vertex u) {
      V[u].delta = V[u].sigma * reduce(delayed::map(G[u], [&] (vertex v) {
        return (V[v].level == i+1) ? 1/V[v].sigma * (1 + V[v].delta) : 0.0;}));});
  }

  return map(V, [] (vtx& v) {return v.delta;});
}
