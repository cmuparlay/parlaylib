#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "star_connectivity.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using vertex = int;
using edge = std::pair<vertex,vertex>;
using edges = parlay::sequence<edge>;
using utils = graph_utils<vertex>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: star_connectivity <n> || star_connectivity <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    edges E;
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
    E = parlay::random_shuffle(E);
    utils::print_graph_stats(E,n);
    std::pair<parlay::sequence<vertex>, parlay::sequence<vertex>> result;

    // first test the simple version
    {
      parlay::internal::timer t("Time");
      for (int i=0; i < 5; i++) {
	result = star_connectivity_simple(E, n);
	t.next("star_connectivity");
      }
      auto r = parlay::histogram_by_index(result.first, n);
      long loc = (parlay::max_element(r) - r.begin());
      std::cout << "number of components   = " << result.second.size()
		<< "\nlargest component size = " << r[loc] << std::endl;
    }
    // then test the version with edge sampling
    {
      parlay::internal::timer t("Time");
      for (int i=0; i < 5; i++) {
	result = star_connectivity(E, n);
	t.next("star_connectivity (with edge sampling)");
      }
      auto r = parlay::histogram_by_index(result.first, n);
      long loc = (parlay::max_element(r) - r.begin());
      std::cout << "number of components   = " << result.second.size()
		<< "\nlargest component size = " << r[loc] << std::endl;
    }
  }
}
