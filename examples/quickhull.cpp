#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "quickhull.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: quickhull <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_real_distribution<> dis(0.0,1.0);

    // generate n random points in a unit square
    auto points = parlay::tabulate(n, [&] (long i) -> point {
      auto r = gen[i];
      return point{dis(r), dis(r)};});

    intseq results;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      results = upper_hull(points);
      t.next("quickhull");
    }

    std::cout << "number of points in upper hull = " << results.size() << std::endl;
  }
}
