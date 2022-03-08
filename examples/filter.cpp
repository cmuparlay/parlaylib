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
    auto result = filter(parlay::iota(n+1), [] (long i) { return i%2 == 0; });
    std::cout << "number of even integers up to n: " << result.size() << std::endl;
  }
}
