#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "mcss.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: mcss <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<int> dis(-100, 100);

    // generate n random numbers from -100 .. 100
    auto vals = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r);});

    parlay::internal::timer t("Time");
    int result;
    for (int i=0; i < 5; i++) {
      result = mcss(vals);
      t.next("mcss");
    }

    std::cout << "mcss = " << result << std::endl;
  }
}
