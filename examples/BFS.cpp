#include <cstddef>

#include <atomic>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <random>

#include <parlay/delayed.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>

namespace delayed = parlay::delayed;

// **************************************************************
// Parallel Breadth First Search
// For each vertex returns the parent in the BFS tree.
// The start vertex points to itself, and any unvisited vertices have -1.
// The graph is a sequence of sequences of vertex ids, representing
// the outedges for each vertex.
// **************************************************************

using vertex = int;
using Graph = parlay::sequence<parlay::sequence<vertex>>;

std::pair<parlay::sequence<vertex>,long> BFS(vertex start, const Graph &G) {
  size_t n = G.size();
  auto parent = parlay::tabulate<std::atomic<vertex>>(n, [&] (size_t i) { return -1; });
  parent[start] = start;
  parlay::sequence<vertex> frontier(1,start);
  long rounds = 0;

  while (frontier.size() > 0) {
    rounds++;

    // get out edges of the frontier and flatten
    auto nested_edges = parlay::map(frontier, [&] (vertex u) {
	return parlay::delayed_tabulate(G[u].size(), [&, u] (size_t i) {
	    return std::pair(u, G[u][i]);});});
    auto edges = delayed::flatten(nested_edges);

    // keep the v from (u,v) edges that succeed in setting the parent array at v to u
    auto edge_f = [&] (auto&& u_v) -> std::optional<vertex> {
      vertex expected = -1;
      auto [u, v] = u_v;
      if ((parent[v] == -1) && parent[v].compare_exchange_strong(expected, u))
        return std::make_optional(v);
      else
        return std::nullopt;
    };
    frontier = delayed::to_sequence(delayed::map_maybe(edges, edge_f));
  }

  // convert from atomic to regular sequence
  return std::make_pair(parlay::map(parent,[] (auto&& x)
  { return x.load(); }), rounds);
}

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate a random graph
// Each vertex has 20 random neighbors (could be self)
// **************************************************************
Graph generate_graph(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<vertex> dis(0, n-1);

  return parlay::tabulate(n, [&] (vertex i) {
    return parlay::tabulate(20, [&] (vertex j) {
      auto r = gen[i*20 + j];
      return dis(r);}, 100);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    auto [result, rounds] = BFS(0, G);
    std::cout << "number of rounds: " << rounds << std::endl;
  }
}
