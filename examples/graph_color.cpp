#include <cstddef>

#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/range.h>
#include <parlay/sequence.h>

#include "graph_color.h"

// **************************************************************
// Driver code
// **************************************************************

// **************************************************************
// Generate a random symmetric (undirected) graph
// Has exponential degree distribution
// **************************************************************
Graph generate_graph(long n) {
  parlay::random_generator gen;
  std::exponential_distribution<float> exp_dis(.5);
  std::uniform_int_distribution<vertex> int_dis(0,n-1);

  // directed edges with self edges removed
  auto E = parlay::flatten(parlay::tabulate(n, [&] (vertex i) {
      auto r = gen[i];
      int m = static_cast<int>(floor(5.*(pow(1.3,exp_dis(r))))) -3;
      return parlay::tabulate(m, [&] (long j) {
	  auto rr = r[j];
	  return std::pair(i,int_dis(rr));});}));
  E = parlay::filter(E, [] (auto e) {return e.first != e.second;});

  // flip in other directions
  auto ET = parlay::map(E, [] (auto e) {
      return std::pair(e.second,e.first);});

  // append together, remove duplicates, and generate undirected (symmetric)
  // adjacency graph
  return parlay::map(parlay::group_by_index(parlay::append(E, ET), n),
		     [] (vertices& v) {return parlay::remove_duplicates(v);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: graph_color <num_vertices>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    parlay::internal::timer t;
    parlay::sequence<int> colors;
    for (int i=0; i < 3; i++) {
      colors = graph_coloring(G);
      t.next("graph color");
    }
    long total_edges = parlay::reduce(parlay::map(G, parlay::size_of()));
    std::cout << "number of edges: " << total_edges  << std::endl;
    int max_color = parlay::reduce(colors, parlay::maximum<int>());
    std::cout << "number of colors: " << max_color + 1  << std::endl;
  }
}
