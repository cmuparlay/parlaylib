#include <limits>
#include <optional>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

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
