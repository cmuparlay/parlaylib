#include <iostream>
#include <random>
#include <string>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "kth_smallest.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: kth_smallest <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<long> dis(0, n-1);

    // generate random long values
    auto data = parlay::tabulate(n, [&] (long i) -> long {
      auto r = gen[i];
      return dis(r); });

    parlay::internal::timer t("Time");
    long result;
    for (int i=0; i < 5; i++) {
      result = kth_smallest(data, n/2);
      t.next("kth_smallest");
    }

    std::cout << "median is: " << result << std::endl;
  }
}
