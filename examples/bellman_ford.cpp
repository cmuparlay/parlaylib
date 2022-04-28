#include <iostream>
#include <string>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "bellman_ford.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using nested_seq = parlay::sequence<parlay::sequence<vertex>>;
using graph = nested_seq;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: bellman_ford <n> || bellman_ford <filename>";
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
    using wtype = float;
    utils::print_graph_stats(G);
    auto WG = utils::add_weights<wtype>(G, 1.0, 20.0);
    parlay::sequence<wtype> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = *bellman_ford_lazy<wtype>(1, WG, WG);
      t.next("bellman_ford");
    }

    double maxd = parlay::reduce(parlay::map(result, [] (auto d) {
                                   return (d == std::numeric_limits<wtype>::max()) ? (wtype) 0 : d;}),
                                 parlay::maximum<wtype>());
    std::cout << "max reachable distance: " << std::setprecision(4) << maxd << std::endl;
  }
}

