#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "spectral_separator.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using utils = graph_utils<vertex>;

  auto usage = "Usage: spectral_separator <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    auto G = utils::grid_graph(n);
    utils::print_graph_stats(G);

    parlay::sequence<bool> partition;
    parlay::internal::timer t("Time");
    for (int i=0; i < 1; i++) {
      partition = partition_graph(G);
      t.next("spectral_separator");
    }

    auto E = utils::to_edges(G);
    long num_in_cut = parlay::count(parlay::map(E, [&] (auto e) {
      return partition[e.first] != partition[e.second];}), true);
    std::cout << "number of edges across the cut: " << num_in_cut << std::endl;
  }
}
