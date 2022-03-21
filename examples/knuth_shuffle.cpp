#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

#include "knuth_shuffle.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: knuth_shuffle <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    auto data = parlay::tabulate(n, [&] (long i) {return i;});
    parlay::sequence<long> result;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = data;
      t.start();
      random_shuffle(result);
      t.next("knuth_shuffle");
    }

    auto first_ten = result.head(std::min(n, 10l));
    std::cout << "first 10 elements: " << parlay::to_chars(first_ten) << std::endl;
  }
}
