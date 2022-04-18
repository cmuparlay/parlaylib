#include <iostream>
#include <string>
#include <random>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "bigint_add.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: bigint_add <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<unsigned int> dis(0,std::numeric_limits<int>::max());

    long m = n/32;

    auto a = parlay::tabulate(m, [&] (long i) {
      auto r = gen[i]; return dis(r);});

    auto b = parlay::tabulate(m, [&] (long i) {
      auto r = gen[i+n]; return dis(r);});

    bigint result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = add(a, b);
      t.next("bigint_add");
    }
    std::cout << n << " bits" << std::endl;
  }
}
