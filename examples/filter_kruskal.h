#include <tuple>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/union_find.h"
#include "helper/speculative_for.h"
#include "kth_smallest.h"

// **************************************************************
// Parallel filtered version of Kruskal's algorithm for MST.
// First runs Kruskal on 2*n lightest edges, then filters
// the remaining and runs Kruskal on those.
// Signicant time savings since don't need to sort all edges.
// For "parallel" Kruskal uses the approach of deterministic reservations, see:
//   "Internally deterministic parallel algorithms can be fast"
//   Blelloch, Fineman, Gibbons, and Shun.
// **************************************************************
template <typename vertex, typename wtype>
using edges = parlay::sequence<std::tuple<vertex,vertex,wtype>>;

// each edge is a tuple<vertex,vertex,weight>
template <typename vertex, typename wtype>
parlay::sequence<long> min_spanning_forest(const edges<vertex,wtype>& E, long n) {
  long m = E.size();
  using indexed_edge = std::tuple<float,long,vertex,vertex>;

  parlay::sequence<bool> in_mst(m, false); // marks if edge i in MST
  union_find<vertex> UF(n);
  parlay::sequence<reservation<long>> R(n); // reservations

  // takes a sequence of edges sorted by weight and runs union_find
  // across them in order
  auto process_edges = [&] (parlay::sequence<indexed_edge>& E) {

    // Find roots of endpoints and reserves them.
    // Earliest edge (the min weight edge since sorted) wins.
    auto reserve = [&] (long i) {
      auto [w, id, u, v] = E[i];
      u = UF.find(u);
      v = UF.find(v);
      if (u != v) {
        R[v].reserve(i);
        R[u].reserve(i);
        return try_commit;
      } else return done;
    };

    // Checks if successfully reserved on at least one endpoint.
    // If so, add edge to mst, and link (union).
    // Note that links cannot form a cycle since on a cycle
    // the maximum edge is not minimum on either side.
    auto commit = [&] (long i) {
      auto [w, id, u, v] = E[i];
      u = UF.find(u);
      v = UF.find(v);
      if (R[v].check(i)) { // won on v
        R[u].check_reset(i);
        UF.link(v, u);     // the assymetric union step
        in_mst[id] = true; // mark as in the mst
        return true;}
      else if (R[u].check(i)) { // won on u
        UF.link(u, v);
        in_mst[id] = true;
        return true; }
      else return false; // lost on both
    };

    // Loop through edges in sorted order (in parallel)
    speculative_for<vertex>(0, E.size(), reserve, commit);
  };

  // find the 2*n smallest edge weight (approximately)
  int k = 1;
  auto sampled_edges = parlay::delayed::tabulate(1 + E.size()/k, [&] (long i) {
    auto [u,v,w] = E[i*k]; return w;});
  double cut_weight = kth_smallest(sampled_edges, 2 * n / k);

  // tag each edge with an index
  auto EI = parlay::delayed_tabulate(m, [&] (long i) {
    auto [u,v,w] = E[i];
    return indexed_edge(w, i, u, v);});

  // Process lightest 2*n edges using Kruskal.
  auto SEI = parlay::sort(filter(EI, [=] (indexed_edge e) {
    auto [w,i,u,v] = e; return w < cut_weight;}));
  process_edges(SEI);

  // Filter rest of edges keeping those whose endpoints are in
  // different components, and process those edge using Kruskal.
  SEI = parlay::sort(filter(EI, [&] (indexed_edge e) {
    auto [w,i,u,v] = e;
    return (UF.find(u) != UF.find(v));}));
  process_edges(SEI);

  // return indices of tree edges
  return parlay::pack_index<long>(in_mst);
}
