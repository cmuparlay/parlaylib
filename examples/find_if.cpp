#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "find_if.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: find_if <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_int_distribution<long> dis(0, n-1);

    // generate n random numbers from 0 .. n-1
    auto vals = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r); });

    long result = ::find_if(vals, [] (long i) { return i == 277; });

    if (result == n) std::cout << "not found" << std::endl;
    else std::cout << "found at location " << result << std::endl;
  }
}
