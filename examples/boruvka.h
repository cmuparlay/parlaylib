#include <atomic>
#include <utility>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>
#include <parlay/utilities.h>

#include "star_connectivity.h"

using vertex = int;
using w_type = float;

using edge = std::pair<vertex,vertex>;
using w_edge = std::pair<edge,w_type>;

parlay::sequence<w_type> boruvka(const parlay::sequence<w_edge>& E,
				 const parlay::sequence<vertex> V,
				 parlay::sequence<std::atomic<w_type>>& W,
				 parlay::sequence<vertex>& P) {
  //std::cout << E.size() << ", " << V.size() << std::endl;

  if (E.size() == 0) return parlay::sequence<w_type>();

  // Write with min the weight to each endpoint
  for_each(V, [&] (vertex v) {W[v] = 1.0;});
  parlay::for_each(E, [&] (w_edge e) {
      auto [u, v] = e.first;
      parlay::write_min(&W[v], e.second, std::less<w_type>());
      parlay::write_min(&W[u], e.second, std::less<w_type>());});
  
  // Keep edges that are min on a vertex
  auto Es = filter(E, [&] (w_edge e) {
      auto [u,v] = e.first;
      return (W[u] == e.second || W[v] == e.second);});
  
  auto V_new = star_contract(map(Es, [] (w_edge e) {return e.first;}),
			     V, P, parlay::random_generator(0));
  
  // update edges to new endpoints and filter out self edges
  auto ES = parlay::delayed::map(E, [&] (w_edge e) {
      auto [u,v] = e.first;
      return w_edge(edge(P[u],P[v]),e.second);});
  auto E_new = parlay::filter(ES, [] (w_edge e) {return e.first.first != e.first.second;});

  auto IE = boruvka(E_new, std::move(V_new), W, P);
  return parlay::append(IE, parlay::map(Es, [] (w_edge e) { return e.second;}));
}   

// each edge is a tuple<vertex,vertex,weight>
parlay::sequence<w_type> min_spanning_forest(const parlay::sequence<w_edge>& E, long n) {
  auto parents = parlay::tabulate(n, [] (vertex i) {return i;});
  auto W = parlay::tabulate<std::atomic<w_type>>(n, [&] (long i) -> w_type {return 0;});
  return boruvka(E, parents, W, parents);
}
