#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "graph_color.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver code
// **************************************************************
using vertex = int;
using graph = parlay::sequence<parlay::sequence<vertex>>;
using utils = graph_utils<vertex>;

bool check(const graph& G, const parlay::sequence<int> colors) {
  auto is_good = [&] (long u) {
    return all_of(G[u], [&] (auto& v) {return colors[u] != colors[v];});};
  return count(parlay::tabulate(G.size(), is_good), true) == G.size();
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: graph_color <num_vertices>";
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
    parlay::sequence<int> colors;
    for (int i=0; i < 3; i++) {
      colors = graph_coloring(G);
      t.next("graph color");
    }
    int max_color = parlay::reduce(colors, parlay::maximum<int>());
    if (!check(G, colors)) std::cout << "bad coloring" << std::endl;
    else std::cout << "number of colors: " << max_color + 1  << std::endl;
  }
}
