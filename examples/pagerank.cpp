#include <iostream>
#include <string>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "pagerank.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  using vertex = int;
  using graph = parlay::sequence<parlay::sequence<vertex>>;

  using element = std::pair<vertex,float>;
  using row = parlay::sequence<element>;
  using sparse_matrix = parlay::sequence<row>;
  using vector = parlay::sequence<double>;

  using utils = graph_utils<vertex>;

  auto usage = "Usage: pagerank <n> || pagerank <filename>";
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
    sparse_matrix M = utils::to_normalized_matrix(G);
    vector v;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      v = pagerank(M, 10);
      t.next("10 iters of pagerank");
    }
    double maxv = *parlay::max_element(v);
    std::cout << "maximum rank = " << maxv * n << std::endl;
  }
}
