#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "cycle_count.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "cycle_count <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    auto permutation = parlay::random_permutation(n);
    parlay::internal::timer t("Time");
    long count;
    for (int i=0; i < 3; i++) {
      count = cycle_count(permutation);
      t.next("Cycle Count");
    }

    std::cout << "number of cycles: " << count << std::endl;
  }
}
