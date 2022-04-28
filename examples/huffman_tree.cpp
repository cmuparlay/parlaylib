#include <iostream>
#include <string>
#include <random>
#include <tuple>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "huffman_tree.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: huffman_tree <num_points>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_real_distribution<float> dis(1.0, static_cast<float>(n));

    // generate unnormalized probabilities
    auto probs = parlay::tabulate(n, [&] (long i) -> float {
      auto r = gen[i];
      return 1.0/dis(r);
    });

    // normalize them
    float total = parlay::reduce(probs);
    probs = parlay::map(probs, [&] (float p) {return p/total;});

    std::tuple<parlay::sequence<node*>,node*> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      t.start();
      result = huffman_tree(probs);
      t.next("huffman_tree");
      delete_tree(std::get<1>(result));
    }

  }
}
