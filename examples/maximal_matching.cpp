#include <cstddef>

#include <iostream>
#include <string>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

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

struct match_step {
  edges const &E;

  // one slot per vertex used for reservation
  parlay::sequence<res> &R;

  // marks a vertex as already matched
  parlay::sequence<bool> &matched;

  // tries to reserve both endpoints with edge i if neither are matched
  bool reserve(edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
    if (matched[u] || matched[v] || (u == v)) return false;
    R[u].reserve(i);
    R[v].reserve(i);
    return true;
  }

  // checks if successfully reserved both endpoints
  // if so mark endpoints as matched and return true
  // otherwise if succeeded on one, reset it
  bool commit(edgeid i) {
    edgeid u = E[i].first;
    edgeid v = E[i].second;
    if (R[v].check(i)) {
      R[v].reset();
      if (R[u].check(i)) {
        matched[u] = matched[v] = true;
        return true;
      }
    } else if (R[u].check(i)) R[u].reset();
    return false;
  }
};

parlay::sequence<edgeid> maximal_matching(edges const &E, long n) {
  size_t m = E.size();

  parlay::sequence<res> R(n);
  parlay::sequence<bool> matched(n, false);
  match_step mStep{E, R, matched};

  speculative_for<edgeid>(mStep, 0, m, 10);

  // returns the edges that successfully committed (their reservation remains in R[v]).
  return parlay::pack(parlay::delayed_seq<edgeid>(n, [&] (size_t i) {return R[i].get();}),
           parlay::tabulate(n, [&] (size_t i) {return R[i].reserved();}));
}

// **************************************************************
// Driver code
// **************************************************************

// **************************************************************
// Generate random edges
// **************************************************************
edges generate_edges(long n, long m) {
  parlay::random rand;

  // create random edges
  auto E = parlay::delayed_tabulate(m, [&] (long i) {
    vertex v1 = rand[2*i]%n;
    vertex v2 = rand[2*i+1]%n;
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
