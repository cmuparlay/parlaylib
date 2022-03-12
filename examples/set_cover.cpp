#include <iostream>
#include <random>
#include <string>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/range.h>

#include "set_cover.h"

// **************************************************************
// Driver
// **************************************************************

// **************************************************************
// Generate random sets
// Exponentially distributed sizes
// **************************************************************
sets generate_sets(long n) {
  parlay::random_generator gen;
  std::exponential_distribution<float> exp_dis(.5);
  std::uniform_int_distribution<idx> int_dis(0,n);

  return parlay::tabulate(n, [&] (idx i) {
      auto r = gen[i];
      int m = static_cast<int>(floor(10.*(pow(1.2,exp_dis(r))))) -5;
      auto elts = parlay::tabulate(m, [&] (long j) {
	  auto rr = r[j];
	  return int_dis(rr);}, 100);
      return parlay::remove_duplicates(elts);});
}

// Check answer is correct (i.e. covers all elements that could be)
bool check(const set_ids& SI, const sets& S, long num_elements) {
  bool_seq a(num_elements, true);

  // set all that could be covered by original sets to false
  parlay::parallel_for(0, S.size(), [&] (long i) {
      for (idx j : S[i]) a[j] = false;});

  // set all that are covered by SI back to true
  parlay::parallel_for(0, SI.size(), [&] (long i) {
      for (idx j : S[SI[i]]) a[j] = true;});

  // check that all are covered
  return (parlay::count(a, true) == num_elements);
}
    
int main(int argc, char* argv[]) {
  auto usage = "Usage: BFS <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    float epsilon = .05;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    sets S = generate_sets(n);
    set_ids r;
    parlay::internal::timer t;
    for (int i=0; i < 3; i++) {
      r = set_cover(S, n, epsilon);
      t.next("set cover");    }
    if (check(r, S, n)) std::cout << "all elements covered!" << std::endl;
    long total = parlay::reduce(parlay::map(S, parlay::size_of()));
    std::cout << "sum of set sizes = " << total << std::endl;
    std::cout << "set cover size = " << r.size() << std::endl;
  }
}

