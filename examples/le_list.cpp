#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "le_list.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using result = parlay::sequence<parlay::sequence<std::pair<size_t, int>>>;
using graph = parlay::sequence<parlay::sequence<int>>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: le_list <n> || le_list <filename>";
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
    result result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = create_le_list(G, GT);
      t.next("le_list");
    }

    long total = reduce(map(result, parlay::size_of()));
    std::cout << "Average LE-list size: " << ((double) total / (double) n) << std::endl;
  }
}
