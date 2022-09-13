#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

using distance = int;
using vertex = int;
#include "le_list.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using result = parlay::sequence<parlay::sequence<std::pair<vertex, distance>>>;
using graph = parlay::sequence<parlay::sequence<vertex>>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: le_list <n> || le_list <filename>";
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
    int seed = 15210;
    parlay::random_generator gen(seed);
    std::uniform_real_distribution<> dis(0.0,1.0);

    // Psuedorandom priorities for the vertices
    auto R = tabulate(n, [&] (vertex i) {
			   auto g = gen[i]; return dis(g); });

    // generate ordering based on priorities
    auto verts = tabulate(n, [&] (vertex i) { return i;});
    auto order = stable_sort(verts, [&] (vertex u, vertex v) {
				      return R[u] < R[v];});
    
    utils::print_graph_stats(G);
    result result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = create_le_list(G, GT, order);
      t.next("le_list");
    }

    long total = reduce(map(result, parlay::size_of()));
    std::cout << "Average LE-list size: " << ((double) total / (double) n) << std::endl;
  }
}
