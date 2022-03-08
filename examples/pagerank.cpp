#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

#include "pagerank.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate a random matrix with 20*n non-zeros
// Columns must sum to 1
// **************************************************************
sparse_matrix generate_matrix(long n) {
  parlay::random_generator gen;
  std::uniform_int_distribution<long> dis(0, n-1);
  int total_entries = n * 20;

  // pick column ids
  auto column_ids = parlay::tabulate(total_entries, [&] (long i) {
    auto r = gen[i];
    return dis(r);});
  auto column_counts = histogram_by_index(column_ids, n);

  // generate each row with 20 entries, nomalized so columsns sum to 1
  return parlay::tabulate(n, [&] (long i) {
    return parlay::tabulate(20, [&] (long j) {
      long c = column_ids[i*20+j];
      return element(c, 1.0/column_counts[c]);},100);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: pagerank <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    sparse_matrix mat = generate_matrix(n);
    auto vec = pagerank(mat, 10);
    double maxv = *parlay::max_element(vec);
    std::cout << "maximum rank = " << maxv * n << std::endl;
  }
}
