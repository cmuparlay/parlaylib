#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>

#include "low_diameter_decomposition.h"
#include "star_connectivity.h"

// **************************************************************
// Graph Connectivity using Low Diameter Decomposition
// **************************************************************
template <typename vertex>
using graph = parlay::sequence<parlay::sequence<vertex>>;

template <typename vertex>
std::pair<parlay::sequence<vertex>,parlay::sequence<vertex>>
ldd_connectivity(const graph<vertex>& G, float beta = .5) {
  long n = G.size();
  using edge = std::pair<vertex,vertex>;

  // Initial Low Diameter Decomposition
  parlay::sequence<vertex> P = LDD(beta, G);

  // Extract the remaining edges
  auto E_r = parlay::flatten(parlay::tabulate(n, [&] (vertex u) {
	      auto remain = filter(G[u], [&] (vertex v) {return P[u] != P[v];});
	      return map(remain, [&] (vertex v) {return edge(P[u],P[v]);}, 1000);}));

  // Extract the remaining vertices
  parlay::sequence<vertex> V_r = parlay::pack_index<vertex>(parlay::tabulate(n, [&] (vertex v) {
							  return P[v] == v;}));

  // Finish off with star contraction on remaining graph
  auto roots = star_contract(E_r, V_r, P, parlay::random_generator(0));

  // Update original vertices
  parlay::parallel_for(0, n, [&] (vertex v) { P[v] = P[P[v]]; });

  return std::pair(std::move(P), std::move(roots));
}
