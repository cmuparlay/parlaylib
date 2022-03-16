#include <iostream>
#include <string>

#include <parlay/sequence.h>

#include "flatten.h"

// **************************************************************
// An implementation of flatten.
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: flatten <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    long sqrt = std::sqrt(n);
    parlay::sequence s(sqrt, parlay::sequence(sqrt, 1l));

    parlay::internal::timer t("Time");
    parlay::sequence<long> result;
    for (int i=0; i < 5; i++) {
      result = ::flatten(s);
      t.next("flatten");
    }

    std::cout << sqrt << "*" << sqrt << " = " << sqrt*sqrt
              <<" elements flattened" << std::endl;
  }
}
