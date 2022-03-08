#include <iostream>
#include <random>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "BFS.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate a random graph
// Each vertex has 10 random neighbors (could be self)
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

int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    parlay::internal::timer t;
    long visited;
    for (int i=0; i < 3; i++) {
      visited = BFS(0, G).second;
      t.next("BFS");
    }
    std::cout << "number of vertices visited: " << visited << std::endl;
  }
}
