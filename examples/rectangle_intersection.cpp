#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"

#include "rectangle_intersection.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: rectangle_intersection <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_real_distribution<float> dis(0.0,1.0);
    float h = .8/std::pow(n, 1.0/3.0);
    
    // generate n random points in a unit square
    // h selected so average of about two intersections per rectangle
    auto rectangles = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      float x = dis(r), y = dis(r), z = dis(r);
      return Bounding_Box{range{x,x+h},range{y,y+h},range{z,z+h}};
    });

    pair_seq r;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      r = rectangle_intersection(rectangles);
      t.next("rectangle_intersection");
    }

    std::cout << "Total number of intersections: " << r.size() << std::endl;
  }
}

