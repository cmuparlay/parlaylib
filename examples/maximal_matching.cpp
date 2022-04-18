#include <iostream>
#include <string>
#include <random>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "maximal_matching.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver code
// **************************************************************
using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: maximal_matching <num_vertices>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    edges E;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      auto G = utils::read_graph_from_file(argv[1]);
      E = utils::to_edges(G);
      n = G.size();
    } else {
      E = utils::rmat_edges(n, 20*n);
      n = utils::num_vertices(E);
    }
    E = parlay::random_shuffle(E);
    utils::print_graph_stats(E,n);
    parlay::sequence<long> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = maximal_matching(E, n);
      t.next("maximal_matching");
    }

    std::cout << "number of matched edges: " << result.size() << std::endl;
  }
}
