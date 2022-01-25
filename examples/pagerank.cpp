#include <limits>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/io.h>
#include <parlay/internal/get_time.h>

using vector = parlay::sequence<double>;
using element = std::pair<long,double>;
using row = parlay::sequence<element>;
using matrix = parlay::sequence<row>;
  
vector mxv(matrix const& mat, vector const& vec) {
  return parlay::map(mat, [&] (row const& r) {
	   return parlay::reduce(parlay::delayed_map(r, [&] (element e) {
		    return vec[e.first] * e.second;}));});
}

vector pagerank(matrix const& mat, int iters) {
  double d = .85; // damping factor
  long n = mat.size();
  vector v(n,1.0/n);
  for (int i=0; i < iters; i++) {
    vector a = mxv(mat, v);
    v = parlay::map(a, [&] (double a) {return (1-d)/n + d * a;});
  }
  return v;
}

// **************************************************************
// Generate a random matrix
// Columns sum to 1
// **************************************************************
matrix generate_matrix(long n) {
  parlay::random rand(17);
  int total_entries = n * 20;
  auto column_ids = parlay::tabulate(total_entries, [&] (long i) {return rand[i]%n;});
  auto column_counts = histogram_by_index(column_ids, n);
  
  // generate each row, nomalized so columsns sum to 1
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
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    matrix mat = generate_matrix(n);
    auto vec = pagerank(mat, 10);
    double maxv = *parlay::max_element(vec);
    std::cout << "maximum rank = " << maxv * n << std::endl;
  }
}
