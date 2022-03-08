#include <atomic>
#include <utility>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/union_find.h"

// **************************************************************
// Find the spanning tree (or forest if not connected) of a graph.
// Uses a commutative version of union-find from file union_find.h
// It is non-deterministic.  Could find different forests on
// different runs.
// **************************************************************

using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;

// **************************************************************
// Given an sequence of edges, i.e. (u,v) pairs, return a spanning
// forest given as indices into the sequence of edges
// **************************************************************
parlay::sequence<long> spanning_forest(edges const &E, vertex n) {
  long m = E.size();
  union_find<vertex> UF(n);

  // initialize to an id out of range
  auto hooks = parlay::tabulate<std::atomic<long>>(n, [&] (size_t i) { return m; });

  parlay::parallel_for (0, m, [&] (long i) {
    vertex u = E[i].first;
    vertex v = E[i].second;
    int cnt = 0;
    bool done = false;
    while(1) {
      u = UF.find(u);
      v = UF.find(v);
      if (u == v) break;
      if (u > v) std::swap(u,v);
      long tmp = m; // compare_.. side effects its argument
      if (hooks[u].load() == m && hooks[u].compare_exchange_strong(tmp, i)) {
        UF.link(u, v);
        break;
      }
    }
  }, 100);

  //get the IDs of the edges in the spanning forest
  auto h = parlay::delayed::map(hooks, [] (auto&& h) {return h.load(); });
  return parlay::filter(h, [&] (size_t a) { return a != m; });
}
