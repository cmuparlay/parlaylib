#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "kcore.h"

// **************************************************************
// Driver
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
      int m = static_cast<int>(floor(5.*(pow(1.4,exp_dis(r))))) -3;
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
  auto usage = "Usage: kcore <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    parlay::internal::timer t;
    parlay::sequence<vertex> degrees;
    for (int i=0; i < 3; i++) {
      degrees = kcore(G);
      t.next("kcore");
    }

    std::cout << "total edges : "
	      << parlay::reduce(parlay::map(G, [] (auto& x) {return x.size();}))
	      << std::endl;
    std::cout << "max degree : "
	      << parlay::reduce(parlay::map(G, [] (auto& x) {return x.size();}), parlay::maximum<size_t>())
	      << std::endl;
    std::cout << "max core (i.e. degeneracy): "
	      << parlay::reduce(degrees, parlay::maximum<vertex>())
	      << std::endl;
  }
}



  
  
