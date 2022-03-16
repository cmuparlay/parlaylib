#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "low_diameter_decomposition.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using vertex = int;
  using graph = parlay::sequence<parlay::sequence<vertex>>;
  using utils = graph_utils<vertex>;

  auto usage = "Usage: low_diameter_decomposition <n> || low_diameter_decomposition <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    graph G, GT;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      G = utils::read_symmetric_graph_from_file(argv[1]);
      GT = G;
      n = G.size();
    } else {
      G = utils::rmat_graph(n, 20*n);
      GT = utils::transpose(G);
    }
    utils::print_graph_stats(G);
    parlay::sequence<vertex> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = LDD<vertex>(.5, G, GT);
      t.next("low_diameter_decomposition");
    }

    auto cluser_ids = parlay::remove_duplicates(result);
    std::cout << "num clusters: " << cluser_ids.size() << std::endl;
  }
}
