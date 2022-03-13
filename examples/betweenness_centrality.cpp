#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/io.h>

#include "betweenness_centrality.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using graph = parlay::sequence<parlay::sequence<vertex>>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: betweenness_centrality <n> || betweenness_centrality <filename>";
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
    parlay::sequence<float> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = BC_single_source(1, G, GT);
      t.next("betweenness_centrality");
    }

    long max_centrality = parlay::reduce(result, parlay::maximum<float>());
    std::cout << "max betweenness centrality = " << max_centrality << std::endl;
  }
}

