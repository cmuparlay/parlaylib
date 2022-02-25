#include <cmath>

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/delayed.h>
#include <parlay/monoid.h>
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
    return parlay::reduce(parlay::map(g, [] (auto& ngh) -> double {return ngh.size();}),
                          parlay::maximum<double>());}
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
    i++;
    vector vnew = normalize(A * (v2 - dot(v2,v1) * v1));
    if (rms_diff(v2, vnew) < error) break;
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
  double error = 1e-7;
  // the first eigenvector
  vector v1 = vector(n, 1.0/sqrt((double) n));
  // and the second
  auto vec = second_eigenvector(mat, v1, error);
  double median = parlay::sort(vec)[n/2];
  return parlay::map(vec, [=] (double x) {return x < median;});
}

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate Graph
// generates a "dumbbell graph".  It consists of two random graphs of
// equal size connected by a single edge.
// The best cut, of course, is across that edge.
// **************************************************************
using edge = std::pair<vertex,vertex>;
auto generate_edges(long n, long m, long off, int seed) {
  parlay::random_generator gen;
  std::uniform_int_distribution<> dis(0, n-1);
  return parlay::tabulate(m, [&] (long i) {
      auto r = gen[i];
      return edge(dis(r) + off, dis(r) + off);});
}

// Converts an edge set into a adjacency representation
// while symmetrizing it, and removing self and redundant edges
auto edges_to_symmetric(const parlay::sequence<edge>& E, long n) {
  auto ET = parlay::map(E, [] (edge e) {return edge(e.second, e.first);});;
  auto G = parlay::group_by_index(parlay::append(E,ET), n);
  return parlay::tabulate(n, [&] (long u) {
    auto x = parlay::filter(G[u], [=] (vertex v) {return u != v;});
    return parlay::remove_duplicates(x);});
}

graph generate_graph(long n) {
  int degree = 5;
  parlay::sequence<parlay::sequence<edge>> E = {
      generate_edges(n/2, degree * n/2, 0, 0), // one side
      generate_edges(n/2, degree * n/2, n/2, 1), // the other
      parlay::sequence<edge>(1,edge(0,n/2))}; // the joining edge
  return edges_to_symmetric(parlay::flatten(E), n);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: spectral_separator <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    auto g = generate_graph(n);
    //std::cout << parlay::to_chars(g) << std::endl;
    auto partition = partition_graph(g);
    long e1 = parlay::reduce(parlay::tabulate(n, [&] (long i) -> long {
      return (i < n/2) != partition[i];}));
    long e2 = parlay::reduce(parlay::tabulate(n, [&] (long i) -> long {
      return (i < n/2) != !partition[i];}));
    std::cout << "percent errors: " << (100.0 * std::min(e1,e2)) / n << std::endl;
  }
}
