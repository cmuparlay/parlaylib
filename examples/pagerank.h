#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// The pagerank algorithm on a sparse graph
// **************************************************************

using vector = parlay::sequence<double>;
using element = std::pair<long,double>;
using row = parlay::sequence<element>;
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
