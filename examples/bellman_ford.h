#include <limits>
#include <optional>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

#include "helper/ligra_light.h"

// **************************************************************
// Parallel Bellman Ford
// Returns an optional, which is empty if there is a negative weight
// cycle, or otherwise returns the distance to each vertex.
// **************************************************************
template <typename wtype, typename vertex, typename weighted_graph>
auto bellman_ford(vertex start, const weighted_graph& GT) {
  long n = GT.size();
  parlay::sequence<wtype> d(n, std::numeric_limits<wtype>::max());
  d[start] = 0.0;

  for (int i=0; i < n; i++) {
    auto dn = parlay::map(GT, [&] (auto& ngh) {
      return parlay::reduce(parlay::delayed_map(ngh, [&] (auto e) {
        return d[e.first] + e.second;}), parlay::minimum<wtype>());});
    dn[start] = 0.0;
    if (dn == d) return std::optional(d);
    d = std::move(dn);
  }
  return std::optional<parlay::sequence<wtype>>();
}

// **************************************************************
// An "Easter Egg"
// Here is a variant of the Bellman-Ford algorithm that only updates
// vertices when one of their in neighbors has changed.
// It uses the standard technique if enough vertices have changed.
// It can be significantly faster.  For example, on a graph with
// unit-weight edges it does O(m) total work, i.e. no more than BFS.
// Needs the transpose graph.
// Yes, it is more complicated than trivial basic version.
// **************************************************************
template <typename wtype, typename vertex, typename weighted_graph>
auto bellman_ford_lazy(vertex start, const weighted_graph& G,
                       const weighted_graph& GT) {
  using edge = std::pair<vertex, wtype>;
  long n = GT.size();
  static constexpr wtype max_d = std::numeric_limits<wtype>::max();

  // used to detect if first to update a distance in a lazy round
  auto visited = parlay::tabulate<std::atomic<bool>>(n, [&] (long i) {
    return (i==start) ? true : false; });

  // distances used when lazy (need to be atomic)
  auto da = parlay::tabulate<std::atomic<wtype>>(n, [&] (long i) {
    return (i==start) ? 0.0 : max_d; });

  // distances used whey greedy
  parlay::sequence<wtype> d(n, max_d);
  d[start] = 0.0;

  // initially just the start is active
  parlay::sequence<vertex> active(1, start);
  long cnt = 0;
  long total = 0;
  bool lazy = true;
  int num_active = 1;

  while (active.size() > 0 && ++cnt <= n) {
    // different threshold if growing or shrinking
    bool do_lazy = ((active.size() < num_active && active.size() * 2 < n)
                    || active.size() * 8 < n);
    num_active = active.size();

    // *** use lazy version if number of active vertices is small enough
    if (do_lazy) {
      // if previous round was greedy, then need to copy from d to da
      if (!lazy)
        parlay::parallel_for(0, n, [&] (long i) {da[i] = d[i];});
      lazy = true;

      // get out edges of the active set along with the distance to the far side
      auto edges = delayed::flatten(parlay::map(active, [&] (vertex u) {
        return delayed::map(G[u], [&,u] (auto e) {
          return std::pair{e.first, da[u] + e.second};});}));

      // add to active for next round if first to reduced distance to v
      active = delayed::to_sequence(delayed::map_maybe(edges, [&] (auto p) {
        auto [v, de] = p;
        bool expected = false;
        return ((parlay::write_min(&da[v], de, std::less<float>())
                 && !visited[v]
                 && visited[v].compare_exchange_strong(expected, true))
                ? std::optional<vertex>{v}
                : std::nullopt);}));

      // clear visited for next round
      parlay::for_each(active, [&] (vertex v) { visited[v] = false;});
    }

      // *** use greedy if number of active vertices is large enough
    else {

      // if previous round was lazy copy from da to d
      if (lazy)
        parlay::parallel_for(0, n, [&] (long i) {d[i] = da[i];});
      lazy = false;

      // same as standard Bellman Ford
      auto dn = parlay::map(GT, [&] (auto& ngh) {
        return parlay::reduce(parlay::delayed_map(ngh, [&] (auto e) {
          return d[e.first] + e.second;}), parlay::minimum<wtype>());});
      dn[start] = 0.0;

      // gather those that have changed into active
      active = parlay::pack_index<vertex>(parlay::delayed_tabulate(n, [&] (long i) {
        return dn[i] != d[i];}));
      d = std::move(dn);
    }
  }

  // if last round was lazy, then copy from da to d
  if (lazy)
    parlay::parallel_for(0, n, [&] (long i) { d[i] = da[i];});
  return (cnt <= n) ? std::optional(std::move(d)) : std::nullopt;
}
