#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: filter <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    parlay::internal::timer t("Time");
    parlay::sequence<long> result;
    for (int i=0; i < 5; i++) {
      result = filter(parlay::iota<long>(n+1), [] (long i) { return i%2 == 0; });
      t.next("filter");
    }

    std::cout << "number of even integers up to n: " << result.size() << std::endl;
  }
}
