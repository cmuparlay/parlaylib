#include <cstddef>
#include <iostream>
#include <string>
#include <tuple>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
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

using vertex = int;
using edge_id = long;
using weighted_edge = std::tuple<vertex,vertex,double>;
using edges = parlay::sequence<weighted_edge>;
using indexed_edge = std::tuple<double,edge_id,vertex,vertex>;
using res = reservation<edge_id>;

parlay::sequence<edge_id> min_spanning_forest(edges &E, long n) {
  size_t m = E.size();

  // tag each edge with an index
  auto EI = parlay::delayed_tabulate(m, [&] (long i) {
    auto [u,v,w] = E[i];
    return indexed_edge(w, i, u, v);});

  auto SEI = parlay::sort(EI);

  parlay::sequence<bool> inMST(m, false); // marks if edge i in MST
  union_find<vertex> UF(n);
  parlay::sequence<res> R(n); // reservations

  // Find roots of endpoints and reserves them.
  // Earliest edge (the min weight edge since sorted) wins.
  auto reserve = [&] (edge_id i) {
    auto [w, id, u, v] = SEI[i];
    u = std::get<2>(SEI[i]) = UF.find(u);
    v = std::get<3>(SEI[i]) = UF.find(v);
    if (u != v) {
      R[v].reserve(i);
      R[u].reserve(i);
      return true;
    } else return false;
  };

  // Checks if successfully reserved on at least one endpoint.
  // If so, add edge to mst, and link (union).
  // Note that links cannot form a cycle since on a cycle
  // the maximum edge is not minimum on either side.
  auto commit = [&] (edge_id i) {
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
  return parlay::pack_index<edge_id>(inMST);
}

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> i_dis(0, n-1);
  std::uniform_real_distribution<double> w_dis(0.0, 1e8);

  // create random edges
  auto E = parlay::delayed_tabulate(n*5, [&] (long i) {
    auto r = gen[i];
    return weighted_edge(i_dis(r), i_dis(r), w_dis(r));});

  // remove self edges
  return parlay::filter(E, [] (weighted_edge e) {
    return std::get<0>(e) != std::get<1>(e);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: min_spanning_tree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    edges E = generate_edges(n);
    std::cout << "edges generated, starting MST" << std::endl;
    auto result = min_spanning_forest(E, n);
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
