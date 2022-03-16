#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "BFS_ligra.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
using graph = nested_seq;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS_ligra <n> || BFS_ligra <filename>";
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
    nested_seq result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = BFS(1, G, GT);
      t.next("BFS_ligra");
    }

    long visited = reduce(map(result, parlay::size_of()));
    std::cout << "num vertices visited: " << visited << std::endl;
  }
}
