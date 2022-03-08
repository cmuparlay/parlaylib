#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "maximal_matching.h"

// **************************************************************
// Driver code
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n, long m) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);

  // create random edges
  auto E = parlay::tabulate(m, [&] (long i) {
    auto r = gen[i];
    vertex v1 = dis(r);
    vertex v2 = dis(r);
    if (v1 > v2) std::swap(v1,v2);
    return edge(v1,v2); });

  // remove self edges
  return parlay::filter(E, [] (edge e) {return e.first != e.second;});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: maximal_matching <num_vertices>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    long m = 10*n;
    edges E = generate_edges(n, m);
    std::cout << "edges generated" << std::endl;
    auto result = maximal_matching(E, n);
    std::cout << "number of matched edges: " << result.size() << " out of " << m << std::endl;
  }
}
