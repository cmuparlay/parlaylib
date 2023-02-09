#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/io.h"

#include "knn.h"

// **************************************************************
// Driver
// **************************************************************

// checks 10 random points and returns the number of points with errors
long check(const parlay::sequence<coords>& points, const knn_graph& G, int k) {
  long n = points.size();
  long num_trials = std::min<long>(20, points.size());
  parlay::random_generator gen(27);
  std::uniform_int_distribution<long> dis(0, n-1);
  auto distance_sq = [] (const coords& a, const coords& b) {
    double r = 0.0;
    for (int i = 0; i < dims; i++) {
      double diff = (a[i] - b[i]);
      r += diff*diff; }
    return r; };
  return parlay::reduce(parlay::tabulate(num_trials, [&] (long a) -> long {
    auto r = gen[a];
    idx i = dis(r);
    coords p = points[i];
    auto x = parlay::to_sequence(parlay::sort(parlay::map(points, [&] (auto q) {
      return distance_sq(p, q);})).cut(1,k+1));
    auto y = parlay::reverse(parlay::map(G[i], [&] (long j) {
      return distance_sq(p,points[j]);}));
    return y != x;}));
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: knn <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    int k = 10;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    coord box_size = 1000000000;
    std::uniform_int_distribution<coord> dis(0, box_size);

    // generate n random points in a cube
    auto points = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      coords pnt;
      for (coord& c : pnt) c = dis(r);
      return pnt;
    });
    knn_graph r;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      r = build_knn_graph(points, k);
      t.next("knn");
    }
    if (check(points, r, k) > 0)
      std::cout << "found error" << std::endl;
    else
      std::cout << "generated " << k << " nearest neighbor graph for " << r.size()
                << " points." << std::endl;
  }
}

