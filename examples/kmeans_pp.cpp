#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/delayed.h>

#include "kmeans_pp.h"

// **************************************************************
// Driver
// **************************************************************
double euclidean_squared(const Point& a, const Point& b) {
  return parlay::reduce(parlay::delayed::tabulate(a.size(), [&] (long i) {
    auto d = a[i]-b[i];
    return d*d;}));
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: kmeans_pp <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    int dims = 10;
    int k = 10;
    double epsilon = .005;
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_real_distribution<> dis(0.0,1.0);

    Points pts = parlay::tabulate(n, [&] (long i) {
      return parlay::tabulate(dims, [&] (long j) {
        auto r = gen[i*dims + j];
        return dis(r);});});
    auto [result, rounds] = kmeans(pts, k, euclidean_squared, epsilon);
    std::cout << rounds << " rounds until diff < " << epsilon << std::endl;
  }
}
