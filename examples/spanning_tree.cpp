#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "spanning_tree.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);

  // create random edges
  auto E = parlay::delayed_tabulate(n*5, [&] (long i) {
      auto r = gen[i];
      return edge(dis(r), dis(r));});
 
  // remove self edges
  return parlay::filter(E, [] (edge e) {return e.first != e.second;});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: spanning_tree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    edges E = generate_edges(n);
    std::cout << "edges generated, starting ST" << std::endl;
    auto result = spanning_forest(E, n);
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
