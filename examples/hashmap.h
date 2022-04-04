#include <cstddef>

#include <algorithm>
#include <atomic>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

#include <parlay/alloc.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// A simple concurrent hash-based map
// Supports concurrent linearizable, insert, find and remove.
// size(), keys(), and taken() don't linearize with updates.
// Requires the capacity is specified on construction.
// No more than the capacity distinct keys can ever be added.
// Once a key is added, removing it will empty the value and mark
// the key as deleted, but only a value with the same key can use the
// same slot (i.e. it still counts towards the capacity).
// **************************************************************

template <class K, class V>
struct hashmap {
 private:
  using KV = std::pair<K,V>;
  using index = unsigned long;
  long m;
  index first_index(K k) { return k.hash() % m;}
  index next_index(index h) { return (h + 1 == m) ? 0 : h + 1; }

  // "pointers" add a high bit to indicate the entry is deleted
  static const uint64_t empty_flag = (1ull << 63);
  KV* deref(uint64_t p) {return (KV*) (p & ~empty_flag);}
  bool is_deleted(uint64_t p) {return (bool) (p & empty_flag);}
  uint64_t add_deleted(uint64_t p) {return p | empty_flag;}

  //parlay::type_allocator<KV> kv_alloc;
  parlay::sequence<std::atomic<uint64_t>> H;

 public:
  hashmap(long size)
      : m(100 + static_cast<index>(1.5 * size)),
        H(parlay::tabulate<std::atomic<uint64_t>>(m, [&] (size_t i) {return 0;})) {}

  ~hashmap() {
    parlay::parallel_for(0,m, [&] (long i) {
      uint64_t kv = H[i].load();
      if (kv != 0) {
        deref(kv)->~KV();
        parlay::p_free(deref(kv));
      }},1000);
  }

  bool insert(const K& k, const V& v) {
    auto kv = (uint64_t) parlay::p_malloc(sizeof(KV)); //kv_alloc.alloc();
    new ((KV*) kv) KV{std::move(k), std::move(v)};
    index i = first_index(k);
    long count = 0;
    while (true) {
      if (count++ == std::min(1000l,m))
        throw std::overflow_error("Hash table overfull");
      uint64_t old = H[i].load();
      if (old == 0 &&
          H[i].compare_exchange_strong(old, kv))
        return true; // successfully inserted
      if (deref(old)->first == k) {
        // if same key is in table but deleted, then reuse
        if (is_deleted(old) &&
            H[i].compare_exchange_strong(old, kv)) {
          deref(old)->first.~K();
          parlay::p_free(deref(old));
          return true;
        } else { // already in, so free up kv and return false
          deref(kv)->~KV();
          parlay::p_free(deref(kv));
          return false;
        }
      }
      i = next_index(i);
    }
  }

  std::optional<V> find(const K& k) {
    index i = first_index(k);
    while (true) {
      uint64_t p = H[i].load();
      if (p == 0) return {};
      if (deref(p)->first == k) return deref(p)->second;
      i = next_index(i);
    }
  }

  std::optional<V> remove(const K& k) {
    index i = first_index(k);
    while (true) {
      uint64_t p = H[i].load();
      if (p == 0) return {};
      if (deref(p)->first == k) {
        if (is_deleted(p) ||
            !H[i].compare_exchange_strong(p, add_deleted(p)))
          return {};
        return std::move(deref(p)->second);
      }
      i = next_index(i);
    }
  }

  parlay::sequence<K> keys() {
    auto x =  parlay::filter(parlay::delayed_map(H, [] (auto const &x) {return x.load();}),
                             [&] (uint64_t p) {return p != 0 && !is_deleted(p);});
    return parlay::map(x, [&] (uint64_t p) {return deref(p)->first;});
  }

  size_t size() {
    return parlay::reduce(parlay::delayed_map(H, [&] (auto const &x) -> long {
      uint64_t p = x.load();
      return x != 0 && !is_deleted(x);}));
  }

  size_t taken() {
    return parlay::reduce(parlay::delayed_map(H, [&] (auto const &x) -> long {
      return x.load() != 0;}));
  }
};

