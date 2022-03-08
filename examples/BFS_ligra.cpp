#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>

#include "BFS_ligra.h"

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
