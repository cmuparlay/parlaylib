#include <iostream>
#include <string>
#include <random>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "counting_sort.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: counting_sort <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    long num_buckets = 256;

    parlay::random_generator gen;
    std::uniform_int_distribution<long> dis(0, num_buckets - 1);

    // generate random long values
    auto data = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return dis(r);});
    
    parlay::internal::timer t("Time");
    parlay::sequence<long> result(n);
    for (int i=0; i < 5; i++) {
      t.start();
      counting_sort(data.begin(), data.end(),
		    result.begin(),
		    data.begin(),
		    num_buckets);
      t.next("counting_sort");
    }

    auto first_ten = result.head(10);
    auto last_ten = result.tail(10);
    std::cout << "first 10 elements: " << parlay::to_chars(first_ten) << std::endl;
    std::cout << "last 10 elements: " << parlay::to_chars(last_ten) << std::endl;
  }
}
