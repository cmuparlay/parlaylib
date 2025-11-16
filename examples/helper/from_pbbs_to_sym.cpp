#include <iostream>
#include <string>
#include "graph_utils.h"

// **************************************************************
// Converts from PBBS format symmetric file to compressed format
// **************************************************************
int main(int argc, char* argv[]) {
  using vertex = int;
  using utils = graph_utils<vertex>;

  auto usage = "./from_pbbs_to_sym <infile> <outfile>";
  if (argc != 3) std::cout << usage << std::endl;
  else {
    auto G = utils::read_graph_from_file_pbbs(argv[1]);
    utils::write_symmetric_graph_to_file(G, argv[2]);
  }
}
