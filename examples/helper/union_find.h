#pragma once

#include <parlay/primitives.h>

// The following supports both "link" (a directed union) and "find".
// They are safe to run concurrently as long as there is no cycle among
// concurrent links.   This can be achieved, for example by only linking
// a vertex with lower id into one with higher degree.
// See:  "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun
// for a discussion of link/find.
template <class vertex>
struct union_find {
  parlay::sequence<std::atomic<vertex>> parents;

  bool is_root(vertex u) {
    return parents[u] < 0;
  }

  // initialize n elements all as roots
  union_find(size_t n) :
      parents(parlay::tabulate<std::atomic<vertex>>(n, [] (long) {
        return -1;})) { }

  vertex find(vertex i) {
    if (is_root(i)) return i;
    vertex p = parents[i];
    if (is_root(p)) return p;

    // find root, shortcutting along the way
    do {
      vertex gp = parents[p];
      parents[i] = gp;
      i = p;
      p = gp;
    } while (!is_root(p));
    return p;
  }

  // Version of union that is safe for parallelism
  // when no cycles are created (e.g. only link from larger
  // to smaller vertex).
  // Does not use ranks.
  void link(vertex u, vertex v) {
    parents[u] = v;
  }
};
