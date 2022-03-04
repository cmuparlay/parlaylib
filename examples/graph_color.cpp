#include <cstddef>

#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds an approximate minimum graph vertex coloring,
// i.e., no neighboring vertices can have the same color.
// Based on the greedy degree heuristic: coloring the vertices in
// order by degree (largest first).
// To respect serial ordering, uses "deterministic reservations".
// Returns the same coloring as the sequential algorithm.  See:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun.
// Will generate same matching as a greedy sequential matching.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using Graph = parlay::sequence<vertices>;

parlay::sequence<int> graph_coloring(Graph const &G) {
  // sort vertices by reverse degree
  auto less = [] (auto& a, auto& b) {return a.size() > b.size();};
  auto Gs = parlay::sort(G, less);
  
  // -1 means unknown
  parlay::sequence<int> colors(G.size(), -1);

  // checks all earlier neighbors have been colored
  auto is_ok = [&] (vertex u) {
    for (vertex v : G[u])
      if (v < u && colors[v] == -1) return try_again;
    return try_commit;
  };

  // if so color this vertex
  auto succeeded = [&] (vertex u) {
    auto ngh_colors = parlay::delayed::map(G[u], [&] (vertex v) {
	return colors[v];});
    
    // find a color unused by any neighbor
    int color = -1;
    for (int c : parlay::sort(ngh_colors)) {
      if (c > color + 1) break; // gap in colors, use it
      color = c;
    }
    colors[u] = color + 1;
    return true;
  };

  // loops over the vertices
  speculative_for<vertex>(0, G.size(), is_ok, succeeded);
  return colors;
}

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
  auto usage = "Usage: graph_coloring <num_vertices>";
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
    long total_edges = parlay::reduce(parlay::map(G, [] (auto& x) {return x.size();}));
    std::cout << "number of edges: " << total_edges  << std::endl;
    int max_color = parlay::reduce(colors, parlay::maximum<int>());
    std::cout << "number of colors: " << max_color + 1  << std::endl;
  }
}
