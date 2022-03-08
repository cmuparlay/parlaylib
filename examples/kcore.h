#include <algorithm>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Find the kcore number (degeneracy), and "coreness" of each vertex
// of an undirected graph.
// Uses parallel peeling and is work efficient.
// **************************************************************
using vertex = int;
using vertices = parlay::sequence<vertex>;
using Graph = parlay::sequence<vertices>;
using namespace parlay;

auto kcore(Graph& G) {
  int n = G.size();
  auto done = sequence<bool>(n, false);
  auto d = map(G, [] (auto& ngh) -> vertex {return ngh.size();});
  vertex maxd = reduce(d, maximum<vertex>())+1;
  auto di = tabulate(n, [&] (vertex i) {return std::pair(d[i], i);});
  auto buckets = map(group_by_index(di,maxd), [] (auto& b) {return Graph(1,b);});
  vertex k = 1;

  while (k < maxd) {
    auto b = filter(flatten(buckets[k]),
		    [&] (auto v) { return done[v] ? false : (done[v] = true);});
    buckets[k].clear();
    if (b.size() == 0) {k++; continue;}
    auto ngh = filter(flatten(map(b, [&] (auto v) {return G[v];})),
		      [&] (auto u) {return d[u] > k;});
    auto u = map(histogram_by_key<vertex>(ngh), [&] (auto uc) {
	auto [u, c] = uc;
	d[u] = std::max(k, d[u] - c);
	return std::pair(d[u], u);});
    for_each(group_by_key(u), [&] (auto& dv) {
	auto [d,v] = dv;
	buckets[d].push_back(std::move(v));});
  }
  return d;
}
