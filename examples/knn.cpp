#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"

#include "knn.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: knn <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    int k = 10;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_int_distribution<coord> dis(0,1000000000);

    // generate n random points in a unit square
    auto points = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      coords pnt;
      for (coord& c : pnt) c = dis(r);
      return pnt;
    });
    knn_graph r;
    parlay::internal::timer t;
    for (int i=0; i < 5; i++) {
      r = build_knn_graph(points, k);
      t.next("knn");
    }
    std::cout << k << " nearest neighbor graph for " << r.size()
              << " points" << std::endl;
  }
}

