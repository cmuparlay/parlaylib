#include <cstddef>

#include <atomic>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

// **************************************************************
// Parallel Triangle Counting
// Uses standard approach of ranking edges by degree and then
// directing edges from lower rank (degree) to higher.
// Then each vertex counts number of intersections with its
// out-neighbors' out-neighbors.
// **************************************************************

using vertex = int;
using vertices = parlay::sequence<vertex>;
using Graph = parlay::sequence<vertices>;

long triangle_count(const Graph &G) {
  size_t n = G.size();
  auto sorted = parlay::sort(parlay::tabulate(n, [&] (long i) {
	return std::pair(G[i].size(), i);}));
  parlay::sequence<long> ranks(n);
  parlay::parallel_for(0, n, [&] (long i) {
      ranks[sorted[i].second] = i;});
  
  Graph Gf = parlay::tabulate(n, [&] (vertex u) {
      return parlay::filter(G[u], [&] (vertex v) {
	  return ranks[u] < ranks[v];});});

  auto count_common = [] (const vertices& a, const vertices& b) {
    long i = 0, j = 0;
    long count = 0;
    while (i < a.size() && j < b.size()) {
      if (a[i] == b[j]) i++, j++, count++;
      else if (a[i] < b[j]) i++;
      else j++;
    }
    return count;
  };

  auto count_from_a = [&] (const vertices& a) {
    return parlay::reduce(parlay::delayed::map(a, [&] (vertex v) {
	  return count_common(a, G[v]);}));};

  return parlay::reduce(parlay::map(G, count_from_a));
}

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate a random graph
// Has exponential degree distribution
// **************************************************************

Graph generate_graph(long n) {
  parlay::random_generator gen;
  std::exponential_distribution<float> exp_dis(.5);
  std::uniform_int_distribution<vertex> int_dis(0,n-1);

  // directed edges with self edges removed
  auto E = parlay::flatten(parlay::tabulate(n, [&] (vertex i) {
      auto r = gen[i];
      int m = static_cast<int>(floor(5.*(pow(1.2,exp_dis(r))))) -3;
      return parlay::tabulate(m, [&] (long j) {
	  auto rr = r[j];
	  return std::pair(i,int_dis(rr));});}));
  E = parlay::filter(E, [] (auto e) {return e.first != e.second;});

  // flip in other directions
  auto ET = parlay::map(E, [] (auto e) {
      return std::pair(e.second,e.first);});

  // append together, remove duplicates, and generate undirected (symmetric)
  // adjacency graph
  return parlay::map(parlay::group_by_index(parlay::append(E, ET), n),
		     [] (vertices& v) {return parlay::remove_duplicates(v);});
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: triangle_count <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    Graph G = generate_graph(n);
    parlay::internal::timer t;
    long count;
    for (int i=0; i < 3; i++) {
      count = triangle_count(G);
      t.next("triangle count");
    }
    std::cout << "number of triangles: " << count << std::endl;
  }
}
