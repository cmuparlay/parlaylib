#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"

#include "3d_range.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: 3d_range <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    long box_size = 1000000000;
    std::uniform_int_distribution<coord> dis(0,box_size);
    double radius = 1.0 * box_size/std::pow(n, 1.0/3.0);

    // generate n random points in a cube
    auto points = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      coords pnt;
      for (coord& c : pnt) c = dis(r);
      return pnt;
    });

    ranges r;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      r = in_range(points, radius);
      t.next("3d_range");
    }
    long num_points = parlay::reduce(parlay::map(r, parlay::size_of{}));

    std::cout << "total points within radius: " << num_points << std::endl;
  }
}

