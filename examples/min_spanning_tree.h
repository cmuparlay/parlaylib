#include <tuple>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/union_find.h"
#include "helper/speculative_for.h"

// **************************************************************
// Parallel version of Kruskal's algorithm for MST
// Uses the approach of deterministic reservations, see:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun.
// Sorts the edges and then simulates the same insertion order
// as the sequential version, but allowing for parallelism.
// Earlier edges always win, which is what gives the same
// tree as the sequential version
// **************************************************************
template <typename vertex>
using w_edges = parlay::sequence<std::tuple<vertex,vertex,double>>;

template <typename vertex>
parlay::sequence<long> min_spanning_forest(w_edges<vertex> &E, long n) {
  using indexed_edge = std::tuple<double,long,vertex,vertex>;
  long m = E.size();

  // tag each edge with an index
  auto EI = parlay::delayed_tabulate(m, [&] (long i) {
    auto [u,v,w] = E[i];
    return indexed_edge(w, i, u, v);});

  auto SEI = parlay::sort(EI);

  parlay::sequence<bool> inMST(m, false); // marks if edge i in MST
  union_find<vertex> UF(n);
  parlay::sequence<reservation<long>> R(n); // reservations

  // Find roots of endpoints and reserves them.
  // Earliest edge (the min weight edge since sorted) wins.
  auto reserve = [&] (long i) {
    auto [w, id, u, v] = SEI[i];
    u = std::get<2>(SEI[i]) = UF.find(u);
    v = std::get<3>(SEI[i]) = UF.find(v);
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
    auto [w, id, u, v] = SEI[i];
    if (R[v].check(i)) {
      R[u].check_reset(i);
      UF.link(v, u);     // the assymetric union step
      inMST[id] = true;
      return true;}
    else if (R[u].check(i)) {
      UF.link(u, v);    // the assymetric union step
      inMST[id] = true;
      return true; }
    else return false;
  };

  // Loop through edges in sorted order (in parallel)
  speculative_for<vertex>(0, m, reserve, commit);
  return parlay::pack_index<long>(inMST);
}
