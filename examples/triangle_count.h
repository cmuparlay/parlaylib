#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// Parallel Triangle Counting
// Uses standard approach of ranking edges by degree and then
// directing edges from lower rank (degree) to higher.
// Then each vertex counts number of intersections with its
// out-neighbors' out-neighbors.
// **************************************************************

template <typename vertex>
using graph = parlay::sequence<parlay::sequence<vertex>>;

template <typename vertex>
long triangle_count(const graph<vertex> &G) {
  using vertices = parlay::sequence<vertex>;

  long n = G.size();
  auto ranks = parlay::rank(parlay::map(G, [] (auto& ngh) {
	return ngh.size();}));
  
  graph<vertex> Gf = parlay::tabulate(n, [&] (vertex u) {
      return parlay::filter(G[u], [&] (vertex v) {
	  return ranks[u] < ranks[v];});});

  auto count_common = [] (const vertices& a, const vertices& b) {
    long i = 0, j = 0;
    long count = 0;
    while (i < a.size() && j < b.size()) {
      if (a[i] == b[j]) i++, j++, count++;
      else if (a[i] < b[j]) i++;
      else j++;
    }
    return count;
  };

  auto count_from_a = [&] (const vertices& a) {
    return parlay::reduce(parlay::delayed::map(a, [&] (vertex v) {
	  return count_common(a, G[v]);}));};

  return parlay::reduce(parlay::map(G, count_from_a));
}
