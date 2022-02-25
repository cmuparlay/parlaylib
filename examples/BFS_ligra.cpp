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
#include <parlay/io.h>

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
  int degree = 10;

  return parlay::tabulate(n, [&] (vertex i) {
      return parlay::remove_duplicates(parlay::tabulate(degree, [&] (vertex j) {
	    auto r = gen[i*degree + j];
	    return dis(r);}, 100));});
}

Graph transpose(const Graph& G) {
  auto x = parlay::tabulate(G.size(), [&] (vertex i) {
      return parlay::map(G[i], [=] (auto& ngh) {
	  return std::pair(ngh, i);});});
  return parlay::group_by_index(parlay::flatten(x), G.size());
}
  
int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS_ligra <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    Graph GT = transpose(G);
    long rounds;
    parlay::internal::timer t;
    for (int i=0; i < 3; i++) {
      rounds = BFS(0, G, GT).second;
      t.next("BFS");
    }
    std::cout << "number of vertices visited: " << rounds << std::endl;
  }
}
