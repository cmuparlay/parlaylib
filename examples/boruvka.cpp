#include <iostream>
#include <string>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "boruvka.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using utils = graph_utils<vertex>;

  auto usage = "Usage: boruvka <n> || boruvka <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    parlay::sequence<edge> E;
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

    parlay::random_generator gen;
    std::uniform_real_distribution<w_type> dis(0.0, 1.0);
    auto WE = parlay::tabulate(E.size(), [&] (long i) {
      auto [u,v] = E[i];
      auto r = gen[i];
      return w_edge(edge(u,v), dis(r));});

    parlay::sequence<w_type> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = min_spanning_forest(WE, n);
      t.next("boruvka");
    }
    std::cout << "number of edges in forest: " << result.size() << std::endl;
  }
}
