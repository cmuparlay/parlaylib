#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

#include "minimum_edit_distance.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: minimum_edit_distance <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    // remove approximately 10% random elements from [0,...,n-1]
    // for two sequences
    // edit distance should be about .2*n
    auto gen0 = parlay::random_generator(0);
    auto gen1 = parlay::random_generator(1);
    std::uniform_int_distribution<long> dis(0,10);
    auto s0 = parlay::filter(parlay::iota(n), [&] (long i) {
      auto r = gen0[i]; return dis(r) != 0;});
    auto s1 = parlay::filter(parlay::iota(n), [&] (long i) {
      auto r = gen1[i]; return dis(r) != 0;});

    parlay::internal::timer t("Time");
    long result;
    for (int i=0; i < 5; i++) {
      result = minimum_edit_distance(s0, s1);
      t.next("minimum_edit_distance");
    }

    std::cout << "total operations = " << s0.size() * s1.size() << std::endl;
    std::cout << "edit distance = " << result << std::endl;
  }
}
