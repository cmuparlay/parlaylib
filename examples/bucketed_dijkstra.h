#include <atomic>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

namespace delayed = parlay::delayed;

// **************************************************************
// Bucketed version of Dijkstra's algorithm for single source shortest
// paths.  Maintains the priority Q as integer buckets and sweeps
// through all of them.  All edge weights must be non-negative
// integers.  If the longest shortest path is l, the algorithm has
// cost:
//    Work = O(m + l), Span = O(l * log n)
// Works best for low-diameter graphs with small integer weights.
// The particular code limits l <= n.
// **************************************************************

template <typename vertex, typename weighted_graph>
auto bucketed_dijkstra(vertex start, const weighted_graph& G) {
  using seq = parlay::sequence<vertex>;
  using nested_seq = parlay::sequence<seq>;
  auto distances = parlay::tabulate<std::atomic<int>>(G.size(), [&] (long i) {
    return (i==start) ? 0 : G.size(); });

  // the bucketed "priority queue"
  parlay::sequence<nested_seq> buckets(G.size());
  buckets[0] = nested_seq(1, seq(1, start));
  nested_seq frontiers;
  int max_distance = 0; // maximum distance seen so far

  // sweep through buckets starting at 0
  int d = 0;
  while (d <= max_distance) {
    // get vertices from bucket and check if still min distance
    auto frontier = filter(flatten(buckets[d]), [&] (auto v) {
      return distances[v] == d;});
    frontiers.push_back(frontier);

    if (frontier.size() > 0) {
      // get out edges of the frontier with distance to other side
      auto edges = delayed::flatten(parlay::map(frontier, [&] (vertex u) {
        return delayed::map(G[u], [=] (auto v) {
          return std::pair{d + v.second, v.first};});}));

      // keep edges whose distance reduces the current best
      auto keep = delayed::to_sequence(delayed::filter(edges, [&] (auto p) {
        auto [dv,v] = p;
        return parlay::write_min(&distances[v], dv, std::less<int>());}));

      if (keep.size() > 0) {
        // maximum distance of kept edges
        int max_d = parlay::reduce(delayed::map(keep, [] (auto p) {return p.first;}),
                                   parlay::maximum<int>());

        // group by distance and add to buckets
        nested_seq nb = parlay::group_by_index(keep, max_d+1);
        parlay::parallel_for(0,max_d+1, [&] (long i) {
          if (nb[i].size() > 0) buckets[i].push_back(nb[i]);});
        max_distance = std::max(max_distance, max_d);
      }
    }
    d++;
  }

  return frontiers;
}
