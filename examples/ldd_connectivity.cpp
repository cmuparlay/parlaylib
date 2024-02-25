#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "ldd_connectivity.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  using vertex = int;
  using graph = parlay::sequence<parlay::sequence<vertex>>;
  using utils = graph_utils<vertex>;

  auto usage = "Usage: ldd_connectivity <n> || ldd_connectivity <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    graph G, GT;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      G = utils::read_symmetric_graph_from_file(argv[1]);
      n = G.size();
    } else {
      G = utils::rmat_symmetric_graph(n, 20*n);
    }
    utils::print_graph_stats(G);
    std::pair<parlay::sequence<vertex>, parlay::sequence<vertex>> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = ldd_connectivity(G);
      t.next("ldd_connectivity");
    }

    auto r = parlay::histogram_by_index(result.first, n);
    long loc = (parlay::max_element(r) - r.begin());
    std::cout << "number of components   = " << result.second.size()
	      << "\nlargest component size = " << r[loc] << std::endl;
  }
}
