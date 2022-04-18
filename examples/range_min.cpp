#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "range_min.h"

// **************************************************************
// Driver code
// **************************************************************
using uint = unsigned int;

int main(int argc, char* argv[]) {
  auto usage = "Usage: range_min <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_int_distribution<uint> dis(0,n-1);

    auto vals = parlay::tabulate(n, [&] (long i) {
      return (i < n/2) ? i : n - i - 1;});
    auto queries = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return std::pair{dis(r), dis(r)};});

    parlay::sequence<uint> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      range_min x(vals);
      result = parlay::tabulate(n, [&] (long i) {
        auto [a,b] = queries[i];
        return x.query(a, b);});
      t.next("range_min");
    }

  }
}
