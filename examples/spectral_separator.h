#include <cmath>
#include <iostream>
#include <random>

#include <parlay/delayed.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

// **************************************************************
// Spectral graph separator
// Takes a graph and returns a partition of the vertices in 1/2 with
// the goal of minimizing (approximately) the number of edges cut.  It
// is based on finding an approximation to the eigenvector of second
// smallest eigenvalue for the graph Laplacian matrix L.  This is
// called the Fiedler vector.  It then splits vertices by the highest
// and lowest values in the vector.  The eigenvector is found by using
// the power method on the matrix diag(1 + max_degree) - L.  Negating
// L converts the second smallest into second largest, and the
// diagonal ensures it is positive definite.
// **************************************************************

// **************************************************************
// Vector operations
// **************************************************************
using vector = parlay::sequence<double>;
auto operator*(double c, const vector& v) {
  return parlay::map(v, [=] (double ve) { return c * ve; });}
auto operator-(const vector& v1, const vector& v2) {
  return parlay::tabulate(v1.size(), [&] (long i) { return v1[i]-v2[i]; });}
double dot(const vector& v1, const vector& v2) {
  return parlay::reduce(parlay::delayed_tabulate(v1.size(), [&] (long i) {
    return v1[i]*v2[i]; }));}
double rms_diff(const vector& v1, const vector& v2) {
  auto diff = v1-v2;
  return parlay::reduce(parlay::delayed::map(diff, [&] (double e) { return e*e; }));}
auto normalize(const vector& v) { return (1.0/std::sqrt(dot(v,v))) * v;}
auto rand_vector(long n) {
  parlay::random_generator gen;
  std::uniform_real_distribution<> dis(0.0, 1.0);
  return normalize(parlay::tabulate(n, [&] (long i) {
    auto r = gen[i]; return dis(r); }));}

// **************************************************************
// Graph Laplacian Matrix 
// **************************************************************
using vertex = int;
using neighbors = parlay::sequence<vertex>;
using graph = parlay::sequence<neighbors>;

// Laplacian matrix L has the degrees down the diagonal and -1 in the
// position of every (u,v) edge.  It is symmetric.
// The following supports muliplying the matrix
//    M = diag(max_degree + 1) - L
// by a vector without explicitly forming the matrix, and using the
// graph driectly (saving space and time).

struct laplacian {
  graph g;
  double diag;
  static double max_degree(const graph& g) {
    return parlay::reduce(parlay::map(g, parlay::size_of()), parlay::maximum<double>()); }
  laplacian(const graph& g) : g(g), diag(max_degree(g)+1.0) {}
  vector operator*(vector const& vec) {
    return parlay::tabulate(g.size(), [&] (long u) {
      neighbors& ngh = g[u];
      // contribution from off diagonal
      double x = parlay::reduce(parlay::delayed::map(ngh, [&] (vertex v) {
        return vec[v];}));
      // add contribution from diagonal
      return (diag + -(double) ngh.size()) * vec[u] + x;},100);
  }
  double size() {return g.size();}
};

// **************************************************************
// Graph Partitioning
// **************************************************************

// Finds approximate second eigenvector given the first eigenvector v1.
// Uses the power method while removing the component along v1.
// Abstracted over matrix type allowing the multiply to be implicit.
template <typename Matrix>
auto second_eigenvector(Matrix A, vector v1, double error) {
  long n = A.size();
  vector v2 = rand_vector(n);
  int i = 0;
  while (true) {
    vector vnew = normalize(A * (v2 - dot(v2,v1) * v1));
    if (i++ % 100 == 0 && rms_diff(v2, vnew) < error) break;
    v2 = vnew;
  }
  std::cout << "number of iterations = " << i << std::endl;
  return v2;
}

// partitions graph by finding an approximation of the second
// eigenvector of the graph laplacian (the Fiedler vector).
// It then splits based on the median value of the vector.
auto partition_graph(graph g) {
  long n = g.size();
  laplacian mat(g);
  double error = .5e-7;
  // the first eigenvector
  vector v1 = vector(n, 1.0/sqrt((double) n));
  // and the second
  auto vec = second_eigenvector(mat, v1, error);
  double median = parlay::sort(vec)[n/2];
  return parlay::map(vec, [=] (double x) {return x < median;});
}

