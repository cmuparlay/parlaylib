#ifndef PARLAY_ALLOC_H
#define PARLAY_ALLOC_H

#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "type_traits.h"  // IWYU pragma: keep
#include "utilities.h"

#include "internal/block_allocator.h"
#include "internal/memory_size.h"
#include "internal/pool_allocator.h"

// IWYU pragma: no_forward_declare is_trivially_relocatable

namespace parlay {

// ****************************************
//    default_allocator (uses powers of two as pool sizes)
// ****************************************

namespace internal {

// these are bucket sizes used by the default allocator.
inline std::vector<size_t> default_allocator_sizes() {
  size_t log_min_size = 4;
  size_t log_max_size = parlay::log2_up(getMemorySize() / 64);

  std::vector<size_t> sizes;
  for (size_t i = log_min_size; i <= log_max_size; i++)
    sizes.push_back(size_t{1} << i);
  return sizes;
}

extern inline internal::pool_allocator& get_default_allocator() {
  static internal::pool_allocator default_allocator(internal::default_allocator_sizes());
  return default_allocator;
}

// pair of total currently used space, and total unused space the allocator has in reserve
inline std::pair<size_t,size_t> memory_usage() {
  return get_default_allocator().stats();
}

inline void memory_clear() {
  return get_default_allocator().clear();
}

}  // namespace internal

// ****************************************
// Following Matches the c++ Allocator specification (minimally)
// https://en.cppreference.com/w/cpp/named_req/Allocator
// Can therefore be used for containers, e.g.:
//    std::vector<int, parlay::allocator<int>>
// ****************************************

template <typename T>
struct allocator {
  using value_type = T;
  T* allocate(size_t n) {
#if defined(_OPENMP)
    // OpenMP doesn't work with Parlay's allocators
    return static_cast<T*>(::operator new(n * sizeof(T)));
#else
    return static_cast<T*>(internal::get_default_allocator().allocate(n * sizeof(T)));
#endif
  }
  void deallocate(T* ptr, [[maybe_unused]] size_t n) {
#if defined(_OPENMP)
    ::operator delete(ptr);
#else
    internal::get_default_allocator().deallocate(static_cast<void*>(ptr), n * sizeof(T));
#endif
  }

  constexpr allocator() noexcept { internal::get_default_allocator(); };
  template <class U> /* implicit */ constexpr allocator(const allocator<U>&) noexcept { }
};

template<typename T>
struct is_trivially_relocatable<allocator<T>> : std::true_type {};

template <class T, class U>
bool operator==(const allocator<T>&, const allocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const allocator<T>&, const allocator<U>&) { return false; }

// ****************************************
//  Free allocation functions
//
//  Allocates headered blocks so the user
//  doesn't need to remember the size
// ****************************************

namespace internal {

constexpr inline size_t alloc_size_offset = 1;  // in size_t sized words

// needs to be at least size_offset * sizeof(size_t)
inline size_t alloc_header_size(size_t n) {  // in bytes
  return (n >= 1024) ? 64 : (n & 15) ? 8 : (n & 63) ? 16 : 64;
}

}  // namespace internal

// allocates and tags with a header (8, 16 or 64 bytes) that contains the size
inline void* p_malloc(size_t n) {
#if defined(_OPENMP)
  // OpenMP doesn't work with Parlay's allocators
  return ::operator new(n);
#else
  size_t hsize = internal::alloc_header_size(n);
  void* ptr = internal::get_default_allocator().allocate(n + hsize);
  void* r = (void*) (((char*) ptr) + hsize);
  *(((size_t*) r) - internal::alloc_size_offset) = n; // puts size in header
  return r;
#endif
}

// reads the size, offsets the header and frees
inline void p_free(void* ptr) {
#if defined(_OPENMP)
  ::operator delete(ptr);
#else
  size_t n = *(((size_t*) ptr) - internal::alloc_size_offset);
  size_t hsize = internal::alloc_header_size(n);
  if (hsize > (1ull << 48)) {
    std::cout << "corrupted header in p_free" << std::endl;
    throw std::bad_alloc();
  }
  internal::get_default_allocator().deallocate((void*) (((char*) ptr) - hsize), n + hsize);
#endif
}


// ****************************************
// Static allocator for single items of a given type, e.g.
//   using long_allocator = type_allocator<long>;
//   long* foo = long_allocator::alloc();
//   *foo = (long) 23;
//   long_allocator::free(foo);
// Uses block allocator, and is headerless
// ****************************************

namespace internal {
template<size_t Size, size_t Align>
extern inline block_allocator& get_block_allocator() {
  static block_allocator a(Size, Align);
  return a;
}
}

template <typename T>
class type_allocator {

  static internal::block_allocator& get_allocator() {
    return internal::get_block_allocator<sizeof(T), alignof(T)>();
  }

public:
  static constexpr inline size_t default_alloc_size = 0;
  static constexpr inline bool initialized = true;

  static T* alloc() {
#if defined(_OPENMP)
    // OpenMP doesn't work with Parlay's allocators
    return static_cast<T*>(::operator new(sizeof(T), std::align_val_t{alignof(T)}));
#else
    return static_cast<T*>(get_allocator().alloc());
#endif
  }

  static void free(T* ptr) {
#if defined(_OPENMP)
    ::operator delete(ptr, std::align_val_t{alignof(T)});
#else
    get_allocator().free(static_cast<void*>(ptr));
#endif
  }

  template <typename ... Args>
  static T* allocate(Args... args) {
    T* r = alloc();
    new (r) T(std::forward<Args>(args)...);
    return r;
  }

  static void retire(T* ptr) {
    ptr->~T();
    free(ptr);
  }

  // for backward compatibility
  static void init(size_t, size_t) {};
  static void init() {};
  static void reserve(size_t n = default_alloc_size) { get_allocator().reserve(n); }
  static void finish() { get_allocator().clear(); }
  static size_t block_size () { return get_allocator().get_block_size(); }
  static size_t num_allocated_blocks() { return get_allocator().num_allocated_blocks(); }
  static size_t num_used_blocks() { return get_allocator().num_used_blocks(); }
  static size_t num_used_bytes() { return num_used_blocks() * block_size(); }
  static void print_stats() { get_allocator().print_stats(); }
};


}  // namespace parlay

#endif  // PARLAY_ALLOC_H
