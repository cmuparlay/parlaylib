#include <iostream>
#include <string>
#include <random>
#include <cmath>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "integer_sort.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: integer_sort <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    using int_type = int;
	
    parlay::random_generator gen;
    std::uniform_int_distribution<int_type> dis(0, n-1);
    int bits = std::ceil(std::log2(n));
    
    // generate random long values
    parlay::sequence<int_type> data = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r);});

    parlay::internal::timer t("Time");
    parlay::sequence<int_type> result;
    for (int i=0; i < 5; i++) {
      result.clear();
      t.start();
      result = ::integer_sort(data,bits);
      t.next("integer_sort");
    }

    auto first_ten = result.head(10);
    std::cout << "first 10 elements: " << parlay::to_chars(first_ten) << std::endl;
  }
}
