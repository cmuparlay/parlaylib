#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// The pagerank algorithm on a sparse graph
// **************************************************************

using vector = parlay::sequence<double>;
using element = std::pair<long,double>;
using row = parlay::sequence<element>;

// a sparse matrix
using sparse_matrix = parlay::sequence<row>;

// sparse matrix vector multiplication
auto mxv(sparse_matrix const& mat, vector const& vec) {
  return parlay::map(mat, [&] (row const& r) {
      return parlay::reduce(parlay::delayed::map(r, [&] (element e) {
	    return vec[e.first] * e.second;}));});
}

// the algorithm
vector pagerank(sparse_matrix const& mat, int iters) {
  double d = .85; // damping factor
  long n = mat.size();
  vector v(n,1.0/n);
  for (int i=0; i < iters; i++) {
    auto a = mxv(mat, v);
    v = parlay::map(a, [&] (double a) {return (1-d)/n + d * a;});
  }
  return v;
}

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
