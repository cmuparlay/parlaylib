#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "min_spanning_tree.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> i_dis(0, n-1);
  std::uniform_real_distribution<double> w_dis(0.0, 1e8);

  // create random edges
  auto E = parlay::delayed_tabulate(n*5, [&] (long i) {
    auto r = gen[i];
    return weighted_edge(i_dis(r), i_dis(r), w_dis(r));});

  // remove self edges
  return parlay::filter(E, [] (weighted_edge e) {
    return std::get<0>(e) != std::get<1>(e);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: min_spanning_tree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    edges E = generate_edges(n);
    std::cout << "edges generated, starting MST" << std::endl;
    auto result = min_spanning_forest(E, n);
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
