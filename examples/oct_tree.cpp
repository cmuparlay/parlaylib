#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"

#include "oct_tree.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: oct_tree <n>";
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
    node* r;
    long size;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      t.start();
      r = z_tree(points);
      size = r->size;
      t.next("oct_tree");
      delete_tree(r);
    }
    std::cout << "tree size: " << size << std::endl;
  }
}

