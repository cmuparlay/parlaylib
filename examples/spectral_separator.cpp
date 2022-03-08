#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "spectral_separator.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate Graph
// generates a "dumbbell graph".  It consists of two random graphs of
// equal size connected by a single edge.
// The best cut, of course, is across that edge.
// **************************************************************
using edge = std::pair<vertex,vertex>;
auto generate_edges(long n, long m, long off, int seed) {
  parlay::random_generator gen;
  std::uniform_int_distribution<> dis(0, n-1);
  return parlay::tabulate(m, [&] (long i) {
      auto r = gen[i];
      return edge(dis(r) + off, dis(r) + off);});
}

// Converts an edge set into a adjacency representation
// while symmetrizing it, and removing self and redundant edges
auto edges_to_symmetric(const parlay::sequence<edge>& E, long n) {
  auto ET = parlay::map(E, [] (edge e) {return edge(e.second, e.first);});;
  auto G = parlay::group_by_index(parlay::append(E,ET), n);
  return parlay::tabulate(n, [&] (long u) {
    auto x = parlay::filter(G[u], [=] (vertex v) {return u != v;});
    return parlay::remove_duplicates(x);});
}

graph generate_graph(long n) {
  int degree = 5;
  parlay::sequence<parlay::sequence<edge>> E = {
      generate_edges(n/2, degree * n/2, 0, 0), // one side
      generate_edges(n/2, degree * n/2, n/2, 1), // the other
      parlay::sequence<edge>(1,edge(0,n/2))}; // the joining edge
  return edges_to_symmetric(parlay::flatten(E), n);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: spectral_separator <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    auto g = generate_graph(n);
    auto partition = partition_graph(g);
    long e1 = parlay::reduce(parlay::tabulate(n, [&] (long i) -> long {
      return (i < n/2) != partition[i];}));
    long e2 = parlay::reduce(parlay::tabulate(n, [&] (long i) -> long {
      return (i < n/2) != !partition[i];}));
    std::cout << "percent errors: " << (100.0 * std::min(e1,e2)) / n << std::endl;
  }
}
