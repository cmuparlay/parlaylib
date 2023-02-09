#include <cstddef>

#include <algorithm>
#include <atomic>
#include <optional>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// A simple concurrent hash-based map
// Supports concurrent linearizable, insert, find and remove.
// size(), keys() don't linearize with updates.
// Requires the capacity to be  specified on construction.
// No more than the capacity distinct keys can ever be added.
// Once a key is added, removing it will empty the value and mark
// the key as deleted, but only a value with the same key can use the
// same slot (i.e. it still counts towards the capacity).
// It uses locks, but holds them very briefly.
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

  enum state : char {empty, full, locked, tomb};
  struct entry {
    std::atomic<state> status;
    K key;
    V value;
    entry() : status(empty) {}
  };
  parlay::sequence<entry> H;

 public:
  hash_map(long size, Hash&& hash = {}, Equal&& equal = {}) 
    : m(100 + static_cast<index>(1.5 * size)),
      hash(hash), equal(equal),
      H(parlay::sequence<entry>(m)) {}

  bool insert(const K& k, const V& v) {
    index i = first_index(k);
    long count = 0;
    while (true) {
      state status = H[i].status;
      if (status == full || status == tomb) {
	if (H[i].key == k) {
	  if (status == full) return false;
	  else if (H[i].status.compare_exchange_strong(status, locked)) {
	    H[i].value = v;
	    H[i].status = full;
	    return true;	    
	  }
	} else {
	  if (count++ == std::min(1000l,m)) {
	    std::cout << "Hash table overfull" << std::endl;
	    return false;
	  }
	  i = next_index(i);
	}
      } else if (status == empty) {
	if (H[i].status.compare_exchange_strong(status, locked)) {
	  H[i].key = k;
	  H[i].value = v;
	  H[i].status = full;
	  return true;
	}
      }
    }
  }

  std::optional<V> find(const K& k) {
    index i = first_index(k);
    while (true) {
      if (H[i].status != full) return {};
      if (H[i].key == k) return H[i].value;
      i = next_index(i);
    }
  }

  std::optional<V> remove(const K& k) {
    index i = first_index(k);
    while (true) {
      if (H[i].status == empty || H[i].status == locked) return {};
      if (H[i].key == k) {
	state old = full;
	if (H[i].status == full &&
	    H[i].status.compare_exchange_strong(old, tomb)) 
	  return std::move(H[i].value);
	else return {};
      }
      i = next_index(i);
    }
  }

  parlay::sequence<K> keys() {
    return parlay::map_maybe(H, [] (const entry &x) {
	return (x.status == full) ? std::optional{x.key} : std::optional<K>{};});
  }

  size_t size() {
    return parlay::reduce(parlay::delayed_map(H, [&] (auto const &x) -> long {
	  return x.status == full;}));
  }
};

