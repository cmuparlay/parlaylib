
#ifndef PARLAY_UTILITIES_H_
#define PARLAY_UTILITIES_H_

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <iostream>
#include <memory>
#include <type_traits>

#include "parallel.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "internal/debug_uninitialized.h"
#endif  // PARLAY_DEBUG_UNINITIALIZED

namespace parlay {

template <typename Lf, typename Rf>
static void par_do_if(bool do_parallel, Lf left, Rf right, bool cons = false) {
  if (do_parallel)
    par_do(left, right, cons);
  else {
    left();
    right();
  }
}

template <typename Lf, typename Mf, typename Rf>
inline void par_do3(Lf left, Mf mid, Rf right) {
  auto left_mid = [&]() { par_do(left, mid); };
  par_do(left_mid, right);
}

template <typename Lf, typename Mf, typename Rf>
static void par_do3_if(bool do_parallel, Lf left, Mf mid, Rf right) {
  if (do_parallel)
    par_do3(left, mid, right);
  else {
    left();
    mid();
    right();
  }
}

template <class T>
size_t log2_up(T);

template <class T>
struct maybe {
  T value;
  bool valid;

  maybe(T v, bool u) : value(v) { valid = u; }
  maybe(T v) : value(v) { valid = true; }
  maybe() { valid = false; }

  bool operator!() const { return !valid; }
  operator bool() const { return valid; };
  T& operator*() { return value; }
};

struct empty {};

typedef uint32_t flags;
const flags no_flag = 0;
const flags fl_sequential = 1;
const flags fl_debug = 2;
const flags fl_time = 4;
const flags fl_conservative = 8;
const flags fl_inplace = 16;

template <typename T>
inline void assign_uninitialized(T& a, const T& b) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with assign_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(b);
}

template <typename T>
inline void assign_uninitialized(T& a, T&& b) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with assign_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));
}

template <typename T>
inline void move_uninitialized(T& a, T& b) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with move_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));
}


// a 32-bit hash function
inline uint32_t hash32(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint32_t hash32_2(uint32_t a) {
  uint32_t z = (a + 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

inline uint32_t hash32_3(uint32_t a) {
  uint32_t z = a + 0x9e3779b9;
  z ^= z >> 15;  // 16 for murmur3
  z *= 0x85ebca6b;
  z ^= z >> 13;
  z *= 0xc2b2ae3d;  // 0xc2b2ae35 for murmur3
  return z ^= z >> 16;
}

// from numerical recipes
inline uint64_t hash64(uint64_t u) {
  uint64_t v = u * 3935559000370003845ul + 2691343689449507681ul;
  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >> 4;
  v *= 4768777513237032717ul;
  v ^= v << 20;
  v ^= v >> 41;
  v ^= v << 5;
  return v;
}

// a slightly cheaper, but possibly not as good version
// based on splitmix64
inline uint64_t hash64_2(uint64_t x) {
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  x = x ^ (x >> 31);
  return x;
}

template <typename ET>
inline bool atomic_compare_and_swap(ET* a, ET oldval, ET newval) {
  static_assert(sizeof(ET) <= 8, "Bad CAS length");
  if (sizeof(ET) == 1) {
    uint8_t r_oval, r_nval;
    std::memcpy(&r_oval, &oldval, sizeof(ET));
    std::memcpy(&r_nval, &newval, sizeof(ET));
    return __sync_bool_compare_and_swap(reinterpret_cast<uint8_t*>(a), r_oval,
                                        r_nval);
  } else if (sizeof(ET) == 4) {
    uint32_t r_oval, r_nval;
    std::memcpy(&r_oval, &oldval, sizeof(ET));
    std::memcpy(&r_nval, &newval, sizeof(ET));
    return __sync_bool_compare_and_swap(reinterpret_cast<uint32_t*>(a), r_oval,
                                        r_nval);
  } else {  // if (sizeof(ET) == 8) {
    uint64_t r_oval, r_nval;
    std::memcpy(&r_oval, &oldval, sizeof(ET));
    std::memcpy(&r_nval, &newval, sizeof(ET));
    return __sync_bool_compare_and_swap(reinterpret_cast<uint64_t*>(a), r_oval,
                                        r_nval);
  }
}

template <typename E, typename EV>
inline E fetch_and_add(E* a, EV b) {
  volatile E newV, oldV;
  do {
    oldV = *a;
    newV = oldV + b;
  } while (!atomic_compare_and_swap(a, oldV, newV));
  return oldV;
}

template <typename E, typename EV>
inline void write_add(E* a, EV b) {
  // volatile E newV, oldV;
  E newV, oldV;
  do {
    oldV = *a;
    newV = oldV + b;
  } while (!atomic_compare_and_swap(a, oldV, newV));
}

template <typename E, typename EV>
inline void write_add(std::atomic<E>* a, EV b) {
  // volatile E newV, oldV;
  E newV, oldV;
  do {
    oldV = a->load();
    newV = oldV + b;
  } while (!std::atomic_compare_exchange_strong(a, &oldV, newV));
}

template <typename ET, typename F>
inline bool write_min(ET* a, ET b, F less) {
  ET c;
  bool r = 0;
  do
    c = *a;
  while (less(b, c) && !(r = atomic_compare_and_swap(a, c, b)));
  return r;
}

template <typename ET, typename F>
inline bool write_min(std::atomic<ET>* a, ET b, F less) {
  ET c;
  bool r = 0;
  do
    c = a->load();
  while (less(b, c) && !(r = std::atomic_compare_exchange_strong(a, &c, b)));
  return r;
}

template <typename ET, typename F>
inline bool write_max(ET* a, ET b, F less) {
  ET c;
  bool r = 0;
  do
    c = *a;
  while (less(c, b) && !(r = atomic_compare_and_swap(a, c, b)));
  return r;
}

template <typename ET, typename F>
inline bool write_max(std::atomic<ET>* a, ET b, F less) {
  ET c;
  bool r = 0;
  do
    c = a->load();
  while (less(c, b) && !(r = std::atomic_compare_exchange_strong(a, &c, b)));
  return r;
}

// returns the log base 2 rounded up (works on ints or longs or unsigned
// versions)
template <class T>
size_t log2_up(T i) {
  assert(i > 0);
  size_t a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    a++;
  }
  return a;
}

inline size_t granularity(size_t n) {
  return (n > 100) ? ceil(pow(n, 0.5)) : 100;
}

/* For inplace sorting / merging, we need to move values around
   rather than making copies. We use tag dispatch to choose between
   moving and copying, so that the move algorithm can be written
   agnostic to which one it uses */

struct move_tag {};
struct uninitialized_move_tag {};
struct copy_tag {};
struct uninitialized_copy_tag {};

// Move dispatch -- move val into dest
template<typename T>
void assign_dispatch(T& val, T& dest, move_tag) {
  dest = std::move(val);
}

// Copy dispatch -- copy val into dest
template<typename T>
void assign_dispatch(const T& val, T& dest, copy_tag) {
  dest = val;
}

// Uninitialized move dispatch -- move construct dest with val
template<typename T>
void assign_dispatch(T& val, T& dest, uninitialized_move_tag) {
  assign_uninitialized(dest, std::move(val));
}

// Uninitialized copy dispatch -- copy initialize dest with val
template<typename T>
void assign_dispatch(const T& val, T& dest, uninitialized_copy_tag) {
  assign_uninitialized(dest, val);
}

/* Tag-dispatch-based assignment that selects between
   initialized/uninitialized copies/moves.
   
   param 3 = std::true_type if a copy should be made, otherwise moves
   param 4 = std::true_type if the destination is uninitialized
*/

template<typename T>
void assign_dispatch(T& dest, T& val, /* move */ std::false_type, /* initialized */ std::false_type) {
  dest = std::move(val);
}

template<typename T>
void assign_dispatch(T& dest, T& val, /* move */ std::false_type, /* uninitialized */ std::true_type) {
  assign_uninitialized(dest, std::move(val));
}

template<typename T>
void assign_dispatch(T& dest, const T& val, /* copy */ std::true_type, /* initialized */ std::false_type) {
  dest = val;
}

template<typename T>
void assign_dispatch(T& dest, const T& val, /* copy */ std::true_type, /* uninitialized */ std::true_type) {
  assign_uninitialized(dest, val);
}

}  // namespace parlay

#endif  // PARLAY_UTILITIES_H_
