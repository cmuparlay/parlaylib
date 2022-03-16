#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/monoid.h>

#include "reduce.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: reduce <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::sequence ones(n,1l);

    parlay::internal::timer t("Time");
    long result;
    for (int i=0; i < 5; i++) {
      result = ::reduce(ones, parlay::plus<long>());
      t.next("reduce");
    }

    std::cout << "sum of ones = " << result << std::endl;
  }
}
