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
// Finds a Maximal Independent Set (MIS).
// Uses "deterministic reservations" to find the lexicographically
// first MIS -- i.e. the one found by the greedy sequential algorithm
// on the given order.   This is the algorithm from:
//   Blelloch, Fineman, and Shun.
//   Greedy Sequential Maximal Independent Set and Matching are Parallel on Average
// Input order should be randomized if not already so.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using Graph = parlay::sequence<vertices>;

parlay::sequence<bool> MIS(Graph const &G) {
  parlay::sequence<bool> in_set(G.size(), false);
  parlay::sequence<bool> decided(G.size(), false);

  // checks all earlier neighbors have been decided
  // and if so and none are in_set, then add to set
  auto check_if_ready = [&] (vertex u) {
    for (vertex v : G[u])
      if (v < u)
	if (!decided[v]) return try_again;
	else if (in_set[v]) return try_commit;
    in_set[u] = true;
    return try_commit;
  };

  // mark as decided
  auto commit = [&] (vertex u) { return decided[u] = true;};

  // loops over the vertices
  speculative_for<vertex>(0, G.size(), check_if_ready, commit);
  return in_set;
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

  auto Eall = parlay::append(std::move(E), std::move(ET));

  // append together, remove duplicates, and generate undirected (symmetric)
  // adjacency graph
  return parlay::map(parlay::group_by_index(std::move(Eall), n),
		     [] (vertices& v) {return parlay::remove_duplicates(v);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: maximal_independent_set <num_vertices>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    parlay::internal::timer t;
    parlay::sequence<bool> in_set;
    for (int i=0; i < 3; i++) {
      in_set = MIS(G);
      t.next("MIS");
    }
    long total_edges = parlay::reduce(parlay::map(G, [] (auto& x) {return x.size();}));
    std::cout << "number of edges: " << total_edges  << std::endl;
    int num_in_set = parlay::reduce(parlay::map(in_set,[] (bool a) {return a ? 1 : 0;}));
    std::cout << "number in set: " << num_in_set << std::endl;
  }
}
