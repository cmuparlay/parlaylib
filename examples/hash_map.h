#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// A simple concurrent hash-based map
// Supports concurrent linearizable, insert, find and remove.
// size(), keys() don't linearize with updates.
// Requires the capacity to be  specified on construction.
// Uses linear probing, so no more than the capacity distinct keys can ever be added.
// Once a key is added, removing it will empty the value and mark
// the key as deleted, but only a value with the same key can use the
// same slot (i.e. it still counts towards the capacity).
//
// Implemented with uses sequence locks to ensure values are updated
// and read atomically.
// Finds are wait free (as long as table is not overfull)
// **************************************************************

template <typename K,
          typename V,
          typename Hash = parlay::hash<K>,
          typename Equal = std::equal_to<>>
struct hash_map {
 private:
  using KV = std::pair<K,V>;
  using index = unsigned long;
  long m;
  index first_index(K k) { return hash(k) % m;}
  index next_index(index h) { return (h + 1 == m) ? 0 : h + 1; }
  Hash hash;
  Equal equal;

  struct entry {
    std::atomic<size_t> seq_num;
    K key;
    V value;
    entry() : seq_num(0) {}
  };
  parlay::sequence<entry> H;

  // uses two low order bits of seq_num to indicate the state
  // 0 = available (only true with 0 sequence number)
  // 1 = full
  // 2 = empty
  // 3 = locked
  // Once an entry it is filled the key never changes, although can be marked as empty
  // State transitions:  0 -> 3 -> 1 -> 3 -> 2 -> 3 -> 1 -> ...
  static constexpr size_t mask = 3;
  static bool is_available(size_t seq_num) {return seq_num == 0;}
  static bool is_full(size_t seq_num) {return (seq_num & mask) == 1;}
  static bool is_empty(size_t seq_num) {return (seq_num & mask) == 2;}
  static bool is_locked(size_t seq_num) {return (seq_num & mask) == 3;}
  static size_t add_full(size_t seq_num) {return (seq_num & ~mask) + 5;}
  static size_t add_empty(size_t seq_num) {return (seq_num & ~mask) + 6;}
  static size_t add_locked(size_t seq_num) {return (seq_num & ~mask) + 7;}

 public:
  hash_map(long size, Hash&& hash = {}, Equal&& equal = {}) 
    : m(100 + static_cast<long>(1.5 * static_cast<float>(size))),
      hash(hash), equal(equal),
      H(parlay::sequence<entry>(m)) {}

  bool insert(const K& k, const V& v) {
    index i = first_index(k);
    long count = 0;
    while (true) {
      size_t seq_num = H[i].seq_num;
      if (is_full(seq_num) || is_empty(seq_num)) {
        if (H[i].key == k) {
          if (is_full(seq_num)) return false; // linearizes on read of seq num
          else if (H[i].seq_num.compare_exchange_strong(seq_num, add_locked(seq_num))) {
            H[i].value = v;
            H[i].seq_num = add_full(seq_num); // linearizes here
            return true;            
          } // else will need to try again, can't linearize if locked
        } else { // occupied by a different key
          if (count++ == std::min(1000l,m)) {
            std::cout << "Hash table overfull" << std::endl;
            return false;
          }
          i = next_index(i);
        }
      } else if (is_available(seq_num)) {
        if (H[i].seq_num.compare_exchange_strong(seq_num, add_locked(seq_num))) {
          H[i].key = k;
          H[i].value = v;
          H[i].seq_num = add_full(seq_num); // linearizes here
          return true;
        }
      }
      // else if locked, try again
    }
  }

  std::optional<V> find(const K& k) {
    index i = first_index(k);
    while (true) {
      size_t seq_num = H[i].seq_num;
      if (is_available(seq_num)) return {};
      if (H[i].key == k) {
        // assumes no upserts (i.e. value goes from full to empty to full ..)
        // inserts linearize at end of lock, and removes at start of lock,
        // so if locked or empty then entry is empty
        if (!is_full(seq_num)) return {};
        V r = H[i].value;
        // ensure value was read atomically
        if (seq_num == H[i].seq_num) return r;
        // if seq_num increased we know cell was empty at some point between reads
        else return {}; 
      } else {
        i = next_index(i);
      }
    }
  }

  std::optional<V> remove(const K& k) {
    index i = first_index(k);
    while (true) {
      size_t seq_num = H[i].seq_num;
      if (is_available(seq_num)) return {}; // linearizes on read of seq num
      else if (is_full(seq_num) || is_empty(seq_num)) {
        if (H[i].key == k) {
          if (is_empty(seq_num)) return {}; // linearizes on read of seq num
          else if (H[i].seq_num.compare_exchange_strong(seq_num, add_locked(seq_num))) {
            // linearizes on successful cas, above (i.e. start of lock period)
            V r = std::move(H[i].value); 
            H[i].seq_num = add_empty(seq_num); 
            return r;
          } // can't linearize if locked, try again
        } else i = next_index(i); // occupied by different key
      }
      // else if locked, then try again
    }
  }

  parlay::sequence<K> keys() {
    return parlay::map_maybe(H, [] (const entry &x) {
        return is_full(x.seq_num) ? std::optional{x.key} : std::optional<K>{};});
  }

  size_t size() {
    return parlay::reduce(parlay::delayed_map(H, [&] (auto const &x) -> long {
        return is_full(x.seq_num);}));
  }
};

