#include <iostream>
#include <string>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>
#include <parlay/internal/get_time.h>

#include "graph_reorder.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using utils = graph_utils<vertex>;

  auto usage = "Usage: graph_reorder <n> || graph_reorder <filename>";
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
      E = utils::to_edges(utils::grid_graph(n));
      n = utils::num_vertices(E);
    }
    E = parlay::filter(E, [] (edge e) {auto [u,v] = e; return (u < v);});
    utils::print_graph_stats(E,n);

    parlay::sequence<vertex> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = graph_reorder(E, n);
      t.next("graph_reorder");
    }

    // check that result is a permutation
    auto check = parlay::sort(result);
    for (int i = 0; i < check.size(); i++)
      if (check[i] != i) {
        std::cout << "mismatch at " << i << std::endl;
        for (int i = 0; i < std::min<int>(n, 100); i++) std::cout << check[i] << ", ";
        std::cout << std::endl;
        abort();
      }

    // calculate average log-difference of edges
    auto x = parlay::map(E, [&] (edge e) {
               return std::log2(std::abs(result[e.first]-result[e.second]));});
    std::cout << "OK: " << (parlay::reduce(x)/E.size()) << std::endl;
  }
}
