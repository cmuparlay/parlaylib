#include <cstddef>

#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

#include "helper/speculative_for.h"

// **************************************************************
// Finds a maximal matching for a graph.
// Uses "deterministic reservations", see:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun.
// Will generate same matching as a greedy sequential matching.
// **************************************************************

using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;
using edgeid = long;
using res = reservation<edgeid>;

parlay::sequence<edgeid> maximal_matching(edges const &E, long n) {
  size_t m = E.size();

  parlay::sequence<res> R(n);
  parlay::sequence<bool> matched(n, false);

  // tries to reserve both endpoints with edge i if neither are matched
  auto reserve = [&] (edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
    if (matched[u] || matched[v] || (u == v)) return false;
    R[u].reserve(i);
    R[v].reserve(i);
    return true;
  };

  // checks if successfully reserved both endpoints
  // if so mark endpoints as matched and return true
  // otherwise if succeeded on one, reset it
  auto commit = [&] (edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
    if (R[v].check(i)) {
      R[v].reset(); // so only one endpoint has edge id in it
      if (R[u].check(i)) {
        matched[u] = matched[v] = true;
        return true;
      }
    } else if (R[u].check(i)) R[u].reset();
    return false;
  };

  // loops over edged in parallel blocks
  speculative_for<edgeid>(0, m, reserve, commit);

  // returns the edges that successfully committed (their reservation remains in R[v]).
  //return parlay::pack(parlay::delayed_seq<edgeid>(n, [&] (size_t i) {return R[i].get();}),
  //                    parlay::tabulate(n, [&] (size_t i) {return R[i].reserved();}));
  return parlay::pack(parlay::delayed::map(R, [&] (auto& r) {return r.get();}),
                      parlay::tabulate(n, [&] (size_t i) {return R[i].reserved();}));
}

// **************************************************************
// Driver code
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n, long m) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);

  // create random edges
  auto E = parlay::tabulate(m, [&] (long i) {
    auto r = gen[i];
    vertex v1 = dis(r);
    vertex v2 = dis(r);
    if (v1 > v2) std::swap(v1,v2);
    return edge(v1,v2); });

  // remove self edges
  return parlay::filter(E, [] (edge e) {return e.first != e.second;});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: maximal_matching <num_vertices>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    long m = 10*n;
    edges E = generate_edges(n, m);
    std::cout << "edges generated" << std::endl;
    auto result = maximal_matching(E, n);
    std::cout << "number of matched edges: " << result.size() << " out of " << m << std::endl;
  }
}
