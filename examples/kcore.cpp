#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "kcore.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using vertex = int;
  using graph = parlay::sequence<parlay::sequence<vertex>>;
  using utils = graph_utils<vertex>;

  auto usage = "Usage: kcore <n> || kcore <filename>";
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
    parlay::sequence<vertex> degrees;
    for (int i=0; i < 1; i++) {
      degrees = kcore(G);
      t.next("kcore");
    }

    long max_core = parlay::reduce(degrees, parlay::maximum<vertex>());
    std::cout << "max core (i.e. degeneracy): " << max_core << std::endl;
  }
}



  
  
