#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "lubys.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver code
// **************************************************************
using vertex = int;
using graph = parlay::sequence<parlay::sequence<vertex>>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: lubys <n> || lubys <filename>";
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
      n = G.size();
    }
    utils::print_graph_stats(G);
    parlay::internal::timer t("Time");
    parlay::sequence<bool> in_set;
    for (int i=0; i < 5; i++) {
      in_set = luby_MIS(G);
      t.next("lubys");
    }

    // check whether an mis
    bool bad_mis = parlay::any_of(parlay::iota(n), [&] (vertex u) {
      return ((in_set[u] && parlay::any_of(G[u], [&] (vertex v) {
                                                return in_set[v];}))||
              (!in_set[u] && parlay::none_of(G[u], [&] (vertex v) {
                                                return in_set[v];})));});

    if (bad_mis > 0) 
      std::cout << "not a maximal independent set" << std::endl;

    int num_in_set = parlay::reduce(parlay::map(in_set,[] (bool a) {
                                          return a ? 1 : 0;}));
    std::cout << "number in set: " << num_in_set << std::endl;
  }
}
