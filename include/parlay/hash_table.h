
#ifndef PARLAY_HASH_TABLE_H_
#define PARLAY_HASH_TABLE_H_

#include <cstddef>

#include <atomic>
#include <iostream>

#include "delayed_sequence.h"
#include "monoid.h"
#include "parallel.h"
#include "sequence.h"
#include "slice.h"
#include "utilities.h"

#include "internal/sequence_ops.h"

namespace parlay {

// A "history independent" hash table that supports insertion, and searching
// It is described in the paper
//   Julian Shun and Guy E. Blelloch
//   Phase-concurrent hash tables for determinism
//   SPAA 2014: 96-107
// Insertions can happen in parallel
// Searches can happen in parallel
// Deletion can happen in parallel
// but insertions cannot happen in parallel with searches or deletions
// and searches cannot happen in parallel with deletions
// i.e. each of the three types of operations have to happen in phase
template <class HASH>
class hashtable {
 private:
  using eType = typename HASH::eType;
  using kType = typename HASH::kType;
  size_t m;
  eType empty;
  HASH hashStruct;
  sequence<eType> TA;
  using index = size_t;

  static void clear(eType* A, size_t n, eType v) {
    auto f = [&](size_t i) { assign_uninitialized(A[i], v); };
    parallel_for(0, n, f, granularity(n));
  }

  struct notEmptyF {
    eType e;
    notEmptyF(eType _e) : e(_e) {}
    int operator()(eType a) { return e != a; }
  };

  index hashToRange(index h) { return static_cast<index>(static_cast<size_t>(h) % m); }
  index firstIndex(kType v) { return hashToRange(hashStruct.hash(v)); }
  index incrementIndex(index h) { return (h + 1 == m) ? 0 : h + 1; }
  index decrementIndex(index h) { return (h == 0) ? m - 1 : h - 1; }
  bool lessIndex(index a, index b) {
    return (a < b) ? (2 * (b - a) < m) : (2 * (a - b) > m);
  }
  bool lessEqIndex(index a, index b) { return a == b || lessIndex(a, b); }

 public:
  // Size is the maximum number of values the hash table will hold.
  // Overfilling the table could put it into an infinite loop.
  hashtable(size_t size, HASH hashF, double load = 1.5)
    : m(100 + static_cast<size_t>(load * size)),
      empty(hashF.empty()),
      hashStruct(hashF),
      TA(sequence<eType>(m, empty)) {
  }

  ~hashtable() { };

  // prioritized linear probing
  //   a new key will bump an existing key up if it has a higher priority
  //   an equal key will replace an old key if replaceQ(new,old) is true
  // returns 0 if not inserted (i.e. equal and replaceQ false) and 1 otherwise
  bool insert(eType v) {
    index i = firstIndex(hashStruct.getKey(v));
    while (true) {
      eType c = TA[i];
      if (c == empty) {
        if (hashStruct.cas(&TA[i], c, v)) return true;
      } else {
        int cmp = hashStruct.cmp(hashStruct.getKey(v), hashStruct.getKey(c));
        if (cmp == 0) {
          if (!hashStruct.replaceQ(v, c))
            return false;
          else if (hashStruct.cas(&TA[i], c, v))
            return true;
        } else if (cmp < 0)
          i = incrementIndex(i);
        else if (hashStruct.cas(&TA[i], c, v)) {
          v = c;
          i = incrementIndex(i);
        }
      }
    }
  }

  // prioritized linear probing
  //   a new key will bump an existing key up if it has a higher priority
  //   an equal key will replace an old key if replaceQ(new,old) is true
  // returns 0 if not inserted (i.e. equal and replaceQ false) and 1 otherwise
  bool update(eType v) {
    index i = firstIndex(hashStruct.getKey(v));
    while (true) {
      eType c = TA[i];
      if (c == empty) {
        if (hashStruct.cas(&TA[i], c, v)) return true;
      } else {
        int cmp = hashStruct.cmp(hashStruct.getKey(v), hashStruct.getKey(c));
        if (cmp == 0) {
          if (!hashStruct.replaceQ(v, c))
            return false;
          else {
            eType new_val = hashStruct.update(c, v);
            if (hashStruct.cas(&TA[i], c, new_val)) return true;
          }
        } else if (cmp < 0)
          i = incrementIndex(i);
        else if (hashStruct.cas(&TA[i], c, v)) {
          v = c;
          i = incrementIndex(i);
        }
      }
    }
  }

  bool deleteVal(kType v) {
    index i = firstIndex(v);
    int cmp;

    // find first element less than or equal to v in priority order
    index j = i;
    eType c = TA[j];

    if (c == empty) return true;

    // find first location with priority less or equal to v's priority
    while ((cmp = (c == empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c))) <
           0) {
      j = incrementIndex(j);
      c = TA[j];
    }
    while (true) {
      // Invariants:
      //   v is the key that needs to be deleted
      //   j is our current index into TA
      //   if v appears in TA, then at least one copy must appear at or before j
      //   c = TA[j] at some previous time (could now be changed)
      //   i = h(v)
      //   cmp = compare v to key of c (positive if greater, 0 equal, negative
      //   less)
      if (cmp != 0) {
        // v does not match key of c, need to move down one and exit if
        // moving before h(v)
        if (j == i) return true;
        j = decrementIndex(j);
        c = TA[j];
        cmp = (c == empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c));
      } else {  // found v at location j (at least at some prior time)

        // Find next available element to fill location j.
        // This is a little tricky since we need to skip over elements for
        // which the hash index is greater than j, and need to account for
        // things being moved downwards by others as we search.
        // Makes use of the fact that values in a cell can only decrease
        // during a delete phase as elements are moved from the right to left.
        index jj = incrementIndex(j);
        eType x = TA[jj];
        while (x != empty && lessIndex(j, firstIndex(hashStruct.getKey(x)))) {
          jj = incrementIndex(jj);
          x = TA[jj];
        }
        index jjj = decrementIndex(jj);
        while (jjj != j) {
          eType y = TA[jjj];
          if (y == empty || !lessIndex(j, firstIndex(hashStruct.getKey(y)))) {
            x = y;
            jj = jjj;
          }
          jjj = decrementIndex(jjj);
        }

        // try to copy the the replacement element into j
        if (hashStruct.cas(&TA[j], c, x)) {
          // swap was successful
          // if the replacement element was empty, we are done
          if (x == empty) return true;

          // Otherwise there are now two copies of the replacement element x
          // delete one copy (probably the original) by starting to look at jj.
          // Note that others can come along in the meantime and delete
          // one or both of them, but that is fine.
          v = hashStruct.getKey(x);
          j = jj;
          i = firstIndex(v);
        }
        c = TA[j];
        cmp = (c == empty) ? 1 : hashStruct.cmp(v, hashStruct.getKey(c));
      }
    }
  }

  // Returns the value if an equal value is found in the table
  // otherwise returns the "empty" element.
  // due to prioritization, can quit early if v is greater than cell
  eType find(kType v) {
    index h = firstIndex(v);
    eType c = TA[h];
    while (true) {
      if (c == empty) return empty;
      int cmp = hashStruct.cmp(v, hashStruct.getKey(c));
      if (cmp >= 0) {
        if (cmp > 0)
          return empty;
        else
          return c;
      }
      h = incrementIndex(h);
      c = TA[h];
    }
  }

  // returns the number of entries
  size_t count() {
    auto is_full = [&](size_t i) -> size_t { return (TA[i] == empty) ? 0 : 1; };
    return internal::reduce(delayed_seq<size_t>(m, is_full), addm<size_t>());
  }

  // returns all the current entries compacted into a sequence
  sequence<eType> entries() {
    return filter(make_slice(TA),
                  [&] (eType v) { return v != empty; });
  }

  index findIndex(kType v) {
    index h = firstIndex(v);
    eType c = TA[h];
    while (true) {
      if (c == empty) return -1;
      int cmp = hashStruct.cmp(v, hashStruct.getKey(c));
      if (cmp >= 0) {
        if (cmp > 0)
          return -1;
        else
          return h;
      }
      h = incrementIndex(h);
      c = TA[h];
    }
  }

  sequence<index> get_index() {
    auto is_full = [&](const size_t i) -> int {
      if (TA[i] != empty)
        return 1;
      else
        return 0;
    };
    auto x = sequence<index>::from_function(m, is_full);
    internal::scan_inplace(make_slice(x), addm<index>());
    return x;
  }

  // prints the current entries along with the index they are stored at
  void print() {
    std::cout << "vals = ";
    for (size_t i = 0; i < m; i++)
      if (TA[i] != empty) std::cout << i << ":" << TA[i] << ",";
    std::cout << std::endl;
  }
};

// Example for hashing numeric values.
// T must be some integer type
template <class T>
struct hash_numeric {
  using eType = T;
  using kType = T;
  eType empty() { return -1; }
  kType getKey(eType v) { return v; }
  size_t hash(kType v) { return static_cast<size_t>(hash64(v)); }
  int cmp(kType v, kType b) { return (v > b) ? 1 : ((v == b) ? 0 : -1); }
  bool replaceQ(eType, eType) { return 0; }
  eType update(eType v, eType) { return v; }
  bool cas(eType* p, eType o, eType n) {
    // TODO: Make this use atomics properly. This is a quick
    // fix to get around the fact that the hashtable does
    // not use atomics. This will break for types that
    // do not inline perfectly inside a std::atomic (i.e.,
    // any type that the standard library chooses to lock)
    return std::atomic_compare_exchange_strong_explicit(
      reinterpret_cast<std::atomic<eType>*>(p), &o, n, std::memory_order_relaxed, std::memory_order_relaxed);
  }
};

}  // namespace parlay

#endif  // PARLAY_HASH_TABLE_H
