#include <iostream>
#include <string>

#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/sequence.h"

#include "set_cover.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************
using idx = int;  // the index of an element or set
using sets = parlay::sequence<parlay::sequence<idx>>;
using set_ids = parlay::sequence<idx>;
using utils = graph_utils<idx>;

// Check answer is correct (i.e. covers all elements that could be)
bool check(const set_ids& SI, const sets& S, long num_elements) {
  parlay::sequence<bool> a(num_elements, true);

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
  auto usage = "Usage: set_cover <n> || set_cover <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    float epsilon = .05;
    long n = 0;
    sets S;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    if (n == 0) {
      S = utils::read_symmetric_graph_from_file(argv[1]);
      n = S.size();
    } else {
      S = utils::rmat_graph(n, 20*n);
    }
    utils::print_graph_stats(S);
    set_ids r;
    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      r = set_cover<idx>::run(S, n, epsilon);
      t.next("set cover");
    }
    if (check(r, S, n)) std::cout << "all elements covered!" << std::endl;
    std::cout << "set cover size = " << r.size() << std::endl;
  }
}

