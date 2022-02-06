#pragma once

#include <parlay/primitives.h>

// The following supports both "union" that is only safe sequentially
// and "link" that is safe in parallel.  Find is always safe in parallel.
// See:  "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun
// for a discussion of link/find.
template <class vertex>
struct union_find {
  parlay::sequence<vertex> parents;

  bool is_root(vertex u) {
    return parents[u] < 0;
  }

  // initialize n elements all as roots
  union_find(size_t n) : parents(parlay::sequence<vertex>(n, -1)) { }

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
