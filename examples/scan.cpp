#include <iostream>
#include <string>

#include <parlay/sequence.h>
#include <parlay/io.h>
#include <parlay/monoid.h>

#include "scan.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: scan <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::sequence ones(n,1);
    auto [result, total] = ::scan(ones, parlay::plus<int>());

    std::cout << "first 10 elements for scan on 1s: "
	      << parlay::to_chars(parlay::to_sequence(result.head(10)))
	      << std::endl;
  }
}
