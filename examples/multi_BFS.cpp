#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "multi_BFS.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
using graph = nested_seq;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS <n> || BFS <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    graph G;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      G = utils::read_symmetric_graph_from_file(argv[1]);
      n = G.size();
    } else {
      G = utils::rmat_graph(n, 20*n);
    }
    utils::print_graph_stats(G);
    parlay::sequence<node_info> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = multi_BFS(1, G);
      t.next("BFS");
    }

    for (int i=0; i < 5; i++)
      std::cout << std::hex << result[i].visited.load() << ", " << result[i].visited_prev.load() << ", " << (int) result[i].d.load() << std::endl;
    //long visited = parlay::reduce(parlay::map(result, parlay::size_of()));
    //std::cout << "num vertices visited: " << visited << std::endl;
  }
}
