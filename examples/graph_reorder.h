#include <atomic>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include <parlay/delayed.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

using vertex = int;
using w_type = float;
using edge_id = int;

using edge = std::pair<vertex,vertex>;
using w_edge = std::pair<edge,w_type>;
struct tagged_w_type {w_type w; edge_id i;};
bool greater(tagged_w_type a, tagged_w_type b) {
  return (a.w > b.w) ? true : ((a.w == b.w) ? a.i > b.i : false);}

// Uses recursive graph contraction to renumber a graph
//   E : sequence of weighted edges (only needed in one direction)
//   V : remaining vertices
//   Sizes : keeps size of contracted vertices on the way
//           down the recursion, and offsets on the way up
//   W : used for temporary space to write priorities
//   P : used for temporary space to write parent of contracted vertex
//   m : original number of edges (not currently used)
// Idea: each round of contraction identifies edges (u,v) that maximize
//     w(u,v)/(|u||v|) on both u and v.  These edges are contracted.
//     |u| is the number of vertices in the component and
//     w(u,v) is the number of edges between components u and v.
void recursive_reorder(parlay::sequence<w_edge>& E,
                       parlay::sequence<vertex>& V,
                       parlay::sequence<vertex>& Sizes,
                       parlay::sequence<std::atomic<tagged_w_type>>& W,
                       parlay::sequence<vertex>& P,
                       long m) {
  // std::cout << E.size() << ", " << V.size() << std::endl;

  // Base case: need to scan if more than one component
  if (E.size() == 0) { 
    auto vsizes = parlay::tabulate(V.size(), [&] (long i) {return Sizes[V[i]];});
    auto [offsets, sum] = parlay::scan(vsizes);
    parlay::parallel_for(0, V.size(), [&] (long i) {Sizes[V[i]] = offsets[i];});
    return;
  }

  // Write with max into W the priority (w(u,v)/(|u||v|)) to each endpoint
  // Priorities are tagged with id to break ties
  // Must firsrt clear W at all active vertices
  parlay::for_each(V, [&] (vertex& v) {
      W[v].store(tagged_w_type(std::numeric_limits<float>::lowest(),0));});
  parlay::parallel_for(0, E.size(), [&] (edge_id i) {
      auto [u, v] = E[i].first;
      //auto w = tagged_w_type(std::round(std::log2(E[i].second / (Sizes[u] * Sizes[v]))), i);
      auto w = tagged_w_type(E[i].second / (Sizes[u] * Sizes[v]), i);
      parlay::write_min(&W[v], w, greater);
      parlay::write_min(&W[u], w, greater);});

  // Need to tag each edge with an index
  auto tagged_E = parlay::delayed::tabulate(E.size(), [&] (long i) {
                    return std::pair(E[i], i);});

  // Get the matched edges that win on both sides and for those:
  // - update size of u to include size of v
  // - have P[v] point to u
  // - return (u, v, old size of u) for use when returning from recursion
  auto matches = parlay::map_maybe(tagged_E, [&] (auto et) {
      auto [e,i] = et;                                   
      auto [u, v] = e.first;
      if (W[u].load().i == i && W[v].load().i == i) {
        vertex usize = Sizes[u];
        Sizes[u] += Sizes[v];
        P[v] = u;
        return std::optional(std::tuple(u, v, usize));
      }
      return std::optional<std::tuple<vertex,vertex,vertex>>();});

  // Update edge endpoints and remove self edges
  E = parlay::map_maybe(E, [&] (w_edge e) {
        auto [u,v] = e.first;
        vertex pu = P[u];
        vertex pv = P[v];
        if (pu > pv) std::swap(pu,pv); // keep oriented low to high
        if (pu == pv) return std::optional<w_edge>();
        return std::optional(w_edge(edge(pu, pv), e.second));});

  // Combine redundant edges
  E = parlay::reduce_by_key(E);

  // These are the remaining vertices after contraction
  V = parlay::filter(V, [&] (vertex v) {return P[v] == v;});

  // recurse
  recursive_reorder(E, V, Sizes, W, P, m);

  // update Sizes to give right offsets
  parlay::for_each(matches, [&] (auto match) {
        auto [u,v,usize] = match;
        Sizes[v] = Sizes[u] + usize;});
}

// E is a sequence of edges, only needed in one direction
// n is the number of vertices
parlay::sequence<vertex> graph_reorder(parlay::sequence<edge>& E, long n) {
  E = parlay::random_shuffle(E); // randomly permute the edges

  // Initialize the five arguments
  auto WE = parlay::map(E, [&] (edge e) { return w_edge(e,1); });
  auto V = parlay::tabulate(n, [] (vertex i) {return i;});
  parlay::sequence<vertex> Sizes(n, 1);
  parlay::sequence<std::atomic<tagged_w_type>> W(n);
  auto P = V;

  // Call main routine
  recursive_reorder(WE, V, Sizes, W, P, E.size());
  return Sizes;
}
