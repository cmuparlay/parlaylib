#include <iostream>
#include <string>
#include <random>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/random.h>

#include "karatsuba.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: karatsuba <size>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    long m = n/digit_len;

    auto randnum = [] (long m, long seed) {
      parlay::random_generator gen(seed);
      auto maxv = std::numeric_limits<digit>::max();
      std::uniform_int_distribution<digit> dis(0, maxv);
      return parlay::tabulate(m, [&] (long i) {
        auto r = gen[i];
        if (i == m-1) return dis(r)/2; // to ensure it is not negative
        else return dis(r);});
    };

    bigint a = randnum(m, 0);
    bigint b = randnum(m, 1);
    bigint result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = karatsuba(a, b);
      t.next("karatsuba");
    }
  }
}
