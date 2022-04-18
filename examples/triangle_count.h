#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// Parallel Triangle Counting
// Uses standard approach of ranking edges by degree and then
// directing edges from lower rank (degree) to higher.
// Then each vertex counts number of intersections with its
// out-neighbors' out-neighbors.
// **************************************************************

template <typename vertex>
using graph = parlay::sequence<parlay::sequence<vertex>>;

// intersection size of two sorted sequences
// Runs in O(m log (1+n/m)) time where m is length of the smaller
// and n length of the larger.
template <typename Slice>
long intersect_size(Slice a, Slice b) {
  if (a.size() == 0 || b.size() == 0) return 0;
  if (a.size() > b.size()) return intersect_size(b,a);
  if (b.size() < 16 * a.size()) {
    long count = 0;
    auto ai = a.begin(); auto ae = a.end();
    auto bi = b.begin(); auto be = b.end();
    while (ai < ae && bi < be) {
      if (*ai == *bi) ai++, bi++, count++;
      else if (*ai < *bi) ai++;
      else bi++;
    }
    return count;
  }
  long ma = a.size()/2;
  auto mb = std::lower_bound(b.begin(), b.end(), a[ma]) - b.begin();
  int match = (mb < b.size() && b[mb] == a[ma]);
  return (match +
          intersect_size(a.cut(0, ma), b.cut(0, mb)) +
          intersect_size(a.cut(ma + 1, a.size()),
                         b.cut(mb + match, b.size())));
}

template <typename vertex>
long triangle_count(const graph<vertex> &G) {
  using vertices = parlay::sequence<vertex>;

  long n = G.size();
  auto ranks = parlay::rank(parlay::map(G, [] (auto& ngh) {
    return ngh.size();}));

  // orient edges from lower degree (rank) to higher degree (rank)
  graph<vertex> Gf = parlay::tabulate(n, [&] (vertex u) {
    return parlay::filter(G[u], [&] (vertex v) {
      return ranks[u] < ranks[v];});});

  auto count_from_a = [&] (const vertices& a) {
    auto as = parlay::make_slice(a);
    return parlay::reduce(parlay::delayed::map(as, [&] (vertex v) {
      return intersect_size(as, parlay::make_slice(G[v]));}));};

  return parlay::reduce(parlay::map(Gf, count_from_a, 1));
}
