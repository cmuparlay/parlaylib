#include <parlay/primitives.h>
#include <parlay/random.h>
#include "union_find.h"

// **************************************************************
// Find the spanning tree (or forest if not connected) of a graph
// Uses a commutative version of union-find from file union_find.h
// **************************************************************

using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;

// **************************************************************
// Given an sequence of edges, i.e. (u,v) pairs, return the indices
// in the sequence of edges in a spanning_forest
// **************************************************************
parlay::sequence<long> spanning_forest(edges const &E, vertex n) {
  long m = E.size();
  union_find<vertex> UF(n);

  // initialize to an id out of range
  auto hooks = parlay::sequence<std::atomic<long>>::from_function(n, [&] (size_t i) {
      return m;});

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
	if (hooks[u].load() == m &&
	    hooks[u].compare_exchange_strong(tmp, i)) {
	  UF.link(u, v);
	  break;
	}
      }
    }, 100);

  //get the IDs of the edges in the spanning forest
  auto h = parlay::delayed_map(hooks, [] (std::atomic<long> &h) {return h.load();});
  return parlay::filter(h, [&] (size_t a) {return a != m;});
}

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n) {
  parlay::random rand;

  // create random edges
  auto E = parlay::delayed_tabulate(n*5, [&] (long i) {
      vertex v1 = rand[2*i]%n;
      vertex v2 = rand[2*i+1]%n;
      if (v1 > v2) std::swap(v1,v2);
      return edge(v1,v2); });

  // remove self and redundant edges
  auto Ef = parlay::filter(E, [] (edge e) {return e.first != e.second;});
  return parlay::unique(parlay::sort(Ef));
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: spanning_tree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    edges E = generate_edges(n);
    std::cout << "edges generated, starting ST" << std::endl;
    auto result = spanning_forest(E, n);
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
