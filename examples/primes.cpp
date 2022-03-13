#include <iostream>
#include <string>

#include <parlay/sequence.h>

#include "primes.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: primes <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::sequence<long> result = primes(n);
    std::cout << "number of primes: " << result.size() << std::endl;
  }
}
