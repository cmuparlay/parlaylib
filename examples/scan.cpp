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
    parlay::sequence ones(n,1l);

    parlay::internal::timer t("Time");
    std::pair<parlay::sequence<long>,long> result;
    for (int i=0; i < 5; i++) {
      result = ::scan(ones, parlay::plus<long>());
      t.next("scan");
    }

    std::cout << "first 10 elements for scan on 1s: "
              << parlay::to_chars(parlay::to_sequence(result.first.head(10)))
              << std::endl;
  }
}
