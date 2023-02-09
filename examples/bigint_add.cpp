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

    auto rand_num = [] (long m, long seed) {
      parlay::random_generator gen(seed);
      auto maxv = std::numeric_limits<digit>::max();
      std::uniform_int_distribution<digit> dis(0, maxv);
      return parlay::tabulate(m, [&] (long i) {
	  auto r = gen[i];
	  return dis(r);});
    };

    long m = n/digit_len;
    bigint a = rand_num(m, 0);
    bigint b = rand_num(m, 1);
    bigint result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = add(a, b);
      t.next("bigint_add");
    }
    std::cout << n << " bits" << std::endl;
  }
}
