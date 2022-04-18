#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "maximal_independent_set.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver code
// **************************************************************
using vertex = int;
using graph = parlay::sequence<parlay::sequence<vertex>>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: maximal_independent_set <n> || maximal_independent_set <filename>";
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
      G = utils::rmat_symmetric_graph(n, 20*n);
    }
    utils::print_graph_stats(G);
    parlay::internal::timer t("Time");
    parlay::sequence<bool> in_set;
    for (int i=0; i < 5; i++) {
      in_set = MIS(G);
      t.next("MIS");
    }

    int num_in_set = parlay::reduce(parlay::map(in_set,[] (bool a) {return a ? 1 : 0;}));
    std::cout << "number in set: " << num_in_set << std::endl;
  }
}
