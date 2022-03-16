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

    std::cout << "generating all primes up to " << n << std::endl;
    parlay::internal::timer t("Time");
    parlay::sequence<long> result;
    for (int i=0; i < 5; i++) {
      result = primes(n);
      t.next("primes");
    }

    std::cout << "number of primes: " << result.size() << std::endl;
  }
}
