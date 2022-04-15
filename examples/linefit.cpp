#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "linefit.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: linefit <num_points>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    double offset = 1.0;
    double slope = 1.0;
    parlay::random_generator gen;
    std::uniform_real_distribution<> dis(0.0,1.0);

    // generate points on a line
    auto pts = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      double x = dis(r);
      return point(x, offset + x * slope);
    });
    point result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = linefit(pts);
      t.next("linefit");
    }

    std::cout << "offset = " << result.first << " slope = " << result.second << std::endl;
  }
}
