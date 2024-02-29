#include <random>
#include <atomic>

#include <parlay/sequence.h>
#include <parlay/primitives.h>
#include <parlay/delayed.h>
#include <parlay/random.h>
#include <parlay/monoid.h>
#include <parlay/internal/get_time.h>

// **************************************************************
// Luby's algorithm for Maximal Independent Set (MIS).  From:
//   A Simple Parallel Algorithm for the Maximal Independent Set Problem
//   Michael Luby
//   SIAM Journal of Computing, 1986
// This is algorithm A (he also describes an algorithm B)
// It has work O(|E|) in expectation and span O(log^2 |V|) whp.
// **************************************************************

// Note graph is copied since it is mutated internally
template <typename graph>
parlay::sequence<bool> MIS(graph& G_in) {
  using vertex = typename graph::value_type::value_type;
  long n = G_in.size();
  parlay::random_generator gen(0);
  std::uniform_int_distribution<int> dis(0,1000000000);

  // first round uses G_in then uses G_nxt
  graph* G = &G_in;
  graph G_nxt(n);
  
  // Each vertex is either in the set, out of the set, or unknown (initially)
  enum state : char {InSet, OutSet, Unknown};
  auto states = parlay::tabulate<std::atomic<state>>(n, [&] (long i) {
			return Unknown;});

  // initial active vetices (all of them)
  auto V = parlay::tabulate(n, [] (vertex i) {return i;});

  // used for writing the priorities on each round
  parlay::sequence<int> priority(n);

  // Loop until no vertices remain
  while (V.size() > 0) {

    // pick random priorities for the remaining vertices
    for_each(V, [&] (vertex u) {auto r = gen[u]; priority[u] = dis(r);});
  
    // for each remaining vertex, if it has a local max priority then
    // add it to the MIS and remove neighbors
    for_each(V, [&] (vertex u) {
      if (priority[u] > 
	  reduce(parlay::delayed::map((*G)[u], [&] (vertex v) {return priority[v];}),
		 parlay::maxm<vertex>())) {
	states[u] = InSet;
	for_each((*G)[u], [&] (vertex v) {
          if (states[v] == Unknown) states[v] = OutSet;});
      }});
    
    // only keep vertices that are still unknown
    V = parlay::filter(V, [&] (vertex u) {return states[u] == Unknown;});

    // only keep edges for which both endpoints are unknown
    for_each(V, [&] (vertex u) {
      G_nxt[u] = parlay::filter((*G)[u], [&] (vertex v) {
	       return states[v] == Unknown;});});
    
    // advanced state of random number generator
    gen = gen[n];
    G = &G_nxt;
  }

  return parlay::map(states, [] (auto& s) {
	     return s.load() == InSet;});
}
