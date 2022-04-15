#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// The pagerank algorithm on a sparse graph
// **************************************************************

using vector = parlay::sequence<double>;

// sparse matrix vector multiplication
template <typename sparse_matrix>
auto mxv(sparse_matrix const& mat, vector const& vec) {
  return parlay::map(mat, [&] (auto const& r) {
    if (r.size() < 100) {
      double result = 0.0;
      for(auto e : r) result += vec[e.first] * e.second;
      return result;
    }
    return parlay::reduce(parlay::delayed::map(r, [&] (auto e) {
      return vec[e.first] * e.second;}));},100);
}

// the algorithm
template <typename sparse_matrix>
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
