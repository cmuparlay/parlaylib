#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "push_relabel_max_flow.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************


int main(int argc, char* argv[]) {
  using utils = graph_utils<vertex_id>;
  auto usage = "Usage: push_relabel_max_flow <n> || push_relabel_max_flow <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    utils::graph G;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      G = utils::read_symmetric_graph_from_file(argv[1]);
      n = G.size();
    } else {
      G = utils::rmat_symmetric_graph(n, 20*n);
      n = G.size();
    }

    using edge = std::pair<vertex_id,weight>;
    utils::print_graph_stats(G);
    auto GW = utils::add_weights<weight>(G,1,1);
    edges s_e = parlay::tabulate(n/4, [&] (long i) {return edge(i, n);});
    edges t_e = parlay::tabulate(n/4, [&] (long i) {return edge(n - n/4 + i, n);});
    GW.push_back(s_e);
    GW.push_back(t_e);
    parlay::parallel_for(0, n/4, [&] (long i) {
      GW[i].push_back(edge(n, n));
      GW[n - n/4 + i].push_back(edge(n+1, n));
    });

    int result;
    //parlay::internal::timer t("Time");
    for (int i=0; i < 2; i++) {
      result = maximum_flow(GW, n, n+1);
      //t.next("push_relabel_max_flow");
    }

    std::cout << "max flow: " << result << std::endl;
  }
}
