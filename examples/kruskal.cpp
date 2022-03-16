#include <iostream>
#include <string>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "kruskal.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using vertex = int;
  using edges = parlay::sequence<std::pair<vertex,vertex>>;
  using utils = graph_utils<vertex>;

  auto usage = "Usage: min_spanning_tree <n> || min_spanning_tree <filename>";
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
    utils::print_graph_stats(E,n);
    auto WE = utils::add_weights<float>(E);
    parlay::sequence<long> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = min_spanning_forest(WE, n);
      t.next("kruskal");
    }
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
