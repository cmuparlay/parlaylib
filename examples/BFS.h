#include <atomic>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Parallel Breadth First Search
// The graph is a sequence of sequences of vertex ids, representing
// the outedges for each vertex.
// Returns a sequence of sequences, with the ith element corresponding to
// **************************************************************
template <typename vertex, typename graph>
auto BFS_simple(vertex start, const graph& G) {
  using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
  auto visited = parlay::tabulate<std::atomic<bool>>(G.size(), [&] (long i) {
    return (i==start) ? true : false; });

  parlay::sequence<vertex> frontier(1,start);
  nested_seq frontiers;
  while (frontier.size() > 0) {
    frontiers.push_back(frontier);

    // get out edges of the frontier and flatten
    auto out = flatten(map(frontier, [&] (vertex u) {return G[u];}));

    // keep the v that succeed in setting the visited array
    frontier = filter(out, [&] (auto&& v) {
      bool expected = false;
      return (!visited[v]) && visited[v].compare_exchange_strong(expected, true);});
  }

  return frontiers;
}

// **************************************************************
// Same as above but using delayed sequences to improve performance
// Typicaly 2-3x faster
// **************************************************************
#include <parlay/delayed.h>
#include <optional>
namespace delayed = parlay::delayed;

template <typename vertex, typename graph>
auto BFS(vertex start, const graph& G) {
  using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
  auto visited = parlay::tabulate<std::atomic<bool>>(G.size(), [&] (long i) {
    return (i==start) ? true : false; });

  parlay::sequence<vertex> frontier(1,start);
  nested_seq frontiers;
  while (frontier.size() > 0) {
    frontiers.push_back(frontier);

    // get out edges of the frontier and flatten
    auto out = delayed::flatten(map(frontier, [&] (vertex v) {
      return delayed::map(G[v], [=] (vertex u) { return u;});}));

    // keep the v that succeed in setting the visited array
    frontier = delayed::to_sequence(delayed::map_maybe(out, [&] (vertex v) {
      bool expected = false;									    
      if ((!visited[v]) && visited[v].compare_exchange_strong(expected, true))
	return std::optional<vertex>(v);
      return std::optional<vertex>();}));
  }

  return frontiers;
}


