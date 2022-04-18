#include <atomic>
#include <random>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>

// **************************************************************
// Counts the number of distinct cycles in a permutation
// Easy to do sequentially, but requires list contraction in parallel.
// This uses the list contraction algorithm from:
//   Optimal Parallel Algorithms in the Binary-Forking Model
//   G. E. Blelloch, J. T. Fineman, Yan Gu, and Yihan Sun
//   SPAA '20
// Requires linear work (worst case) and O(log n) span whp.
// **************************************************************
struct lnk {
  lnk *prev, *next;
  long p;
  std::atomic<char> degree;
  bool is_leaf;
};

long cycle_count(parlay::sequence<long>& permutation) {
  long n = permutation.size();
  parlay::random_generator gen(0);
  std::uniform_int_distribution<long> dis(0, n-1);
  parlay::sequence<lnk> links(n);
  parlay::parallel_for(0, n, [&] (long i) {
    links[i].next = &links[permutation[i]];
    links[permutation[i]].prev = &links[i];
    auto r = gen[i];
    links[i].p = dis(r);
  });

  parlay::parallel_for(0, n, [&] (long i) {
    links[i].degree = ((links[i].prev->p < links[i].p) +
                       (links[i].next->p <= links[i].p));
    links[i].is_leaf = links[i].degree == 0;});

  auto roots = parlay::tabulate(n, [&] (long i) {
    if (!links[i].is_leaf) return 0;
    lnk* l = &links[i];
    do {
      l->next->prev = l->prev;
      l->prev->next = l->next;
      l = (l->prev->p < l->next->p) ? l->prev : l->next;
    } while (l->prev != l && l->degree.fetch_add(-1) == 1);
    return (l->prev == l) ? 1 : 0;});

  return parlay::reduce(roots);
}
