#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "cartesian_tree.h"

// **************************************************************
// Driver
// **************************************************************

parlay::sequence<long> generate_values(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);

  return parlay::tabulate(n, [&] (long i) {
    auto r = gen[i];
    return dis(r);
  });
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: cartesian_tree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    parlay::sequence<long> values = generate_values(n);
    parlay::sequence<long> parents;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      parents = cartesian_tree(values);
      t.next("Cartesian Tree");
    }

    auto h = parlay::tabulate(n, [&] (long i) {
      long depth = 1;
      while (i != parents[i]) {
        i = parents[i];
        depth++;
      }
      return depth;});

    std::cout << "depth of tree: "
              << parlay::reduce(h, parlay::maximum<long>())
              << std::endl;
  }
}
