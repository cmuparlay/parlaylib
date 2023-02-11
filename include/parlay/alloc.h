#ifndef PARLAY_ALLOC_H
#define PARLAY_ALLOC_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <memory>
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

// ----------------------------------------------------------------------------
//                            internal pool allocator
//
// allocates pools of blocks with powers of two sizes. Used internally to provide
// allocation for the container allocator, parlay::allocator<T>; and for the free
// functions, parlay::p_malloc and parlay::p_free.
// ----------------------------------------------------------------------------

namespace internal {

// The size of the largest pool used by the memory allocator
inline const size_t default_allocator_max_pool_size = getMemorySize() / 64;

// these are bucket sizes used by the default allocator. They are powers
// of two starting at 16 and going until SystemMemory / 64.
//
// Note that them being powers of two is very important for correctness
// of the high-level allocators defined here!
//  - being powers of two means that if a block is suitably aligned for an object
//    of type T, the next block is guaranteed to also be suitably aligned for T
//  - p_malloc optimizes its header by only storing log(size) of the allocation,
//    which works because that is enough information to know which pool it came from
//
inline std::vector<size_t> default_allocator_sizes() {
  size_t log_min_size = 4;
  size_t log_max_size = log2_up(default_allocator_max_pool_size);

  std::vector<size_t> sizes;
  for (size_t i = log_min_size; i <= log_max_size; i++)
    sizes.push_back(size_t{1} << i);
  return sizes;
}

extern inline pool_allocator& get_default_allocator() {
  static pool_allocator default_allocator(default_allocator_sizes());
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

// ----------------------------------------------------------------------------
//  Free allocation functions
//
//  Allocates headered blocks so the user doesn't need to remember the size
// ----------------------------------------------------------------------------

namespace internal {

// Header used by p_malloc. Stores the requested size of the buffer and
// the offset of the underlying buffer that we need to free later.
struct p_malloc_header {
  uint64_t log_size : 8;   // We can store the log of the size because the default
  uint64_t offset : 48;    // allocator uses only powers of two pool sizes
};
static_assert(sizeof(p_malloc_header) <= 8);

// Minimum size of the padding for p_malloc. For large allocations, uses a larger
// padding to guarantee at least 64-byte alignment. Needs to be at least sizeof(size_t).
inline size_t alloc_padding_size(size_t n) {  // in bytes
  return  (n >= 1024) ? 64 : (n & 15) ? 8 : (n & 63) ? 16 : 64;
}

}  // namespace internal


// Allocate size bytes of uninitialized storage. Optionally ask for an
// aligned buffer of memory with the given alignment.
//
// By default, the alignment is at least alignof(std::max_align_t), and may
// be larger for larger size allocations.
inline void* p_malloc(size_t size, size_t align = alignof(std::max_align_t)) {
  assert(align > 0 && (align & (align - 1)) == 0 && "Requested alignment must be a power of two");

  // allocates and tags with a header that contains the size and offset
  size_t pad_size = std::max<size_t>(internal::alloc_padding_size(size), align);
  void* buffer = internal::get_default_allocator().allocate(size + pad_size);
  void* offset_buffer = static_cast<void*>(static_cast<std::byte*>(buffer) + sizeof(internal::p_malloc_header));
  size_t space = size + pad_size - sizeof(internal::p_malloc_header);
  void* new_buffer = std::align(pad_size, size, offset_buffer, space);
  assert(new_buffer != nullptr && new_buffer == offset_buffer);
  size_t offset = (static_cast<std::byte*>(new_buffer) - static_cast<std::byte*>(buffer));
  assert(offset == (size + pad_size) - space);
  new (static_cast<std::byte*>(new_buffer) - sizeof(internal::p_malloc_header))
      internal::p_malloc_header{log2_up(size + pad_size), offset};
  return static_cast<void*>(new_buffer);
}

// Free a block of memory obtained by p_malloc
inline void p_free(void* ptr) {
  // reads the header to determine the offset and size, then frees
  auto h = *from_bytes<internal::p_malloc_header>(static_cast<std::byte*>(ptr) - sizeof(internal::p_malloc_header));
  if (h.log_size > 48u || h.offset > 1ull << 48) {
    std::cerr << "corrupted header in p_free" << std::endl;
    std::abort();
  }
  auto buffer = static_cast<void*>(static_cast<std::byte*>(ptr) - h.offset);
  internal::get_default_allocator().deallocate(buffer, size_t{1} << size_t(h.log_size));
}


// ----------------------------------------------------------------------------
//                            Container allocator
// ----------------------------------------------------------------------------

// A general purpose container allocator for arrays of type T.
//
// Matches the c++ Allocator specification (minimally)
// https://en.cppreference.com/w/cpp/named_req/Allocator
// Can therefore be used for STL containers, e.g.:
//    std::vector<int, parlay::allocator<int>>
//
template <typename T>
struct allocator {
  using value_type = T;
  T* allocate(size_t n) {
    // Use headerless blocks straight from the pool if the alignment is suitable,
    // otherwise fall back to manually-aligned headered blocks from p_malloc
    if constexpr (alignof(T) > internal::pool_allocator::max_alignment) {
      return static_cast<T*>(p_malloc(n * sizeof(T), alignof(T)));
    }
    else {
      // Should be suitably aligned for alignments up to pool_allocator::max_alignment
      void* buffer = internal::get_default_allocator().allocate(n * sizeof(T));
      assert(reinterpret_cast<uintptr_t>(buffer) % alignof(T) == 0);
      return static_cast<T*>(buffer);
    }
  }
  void deallocate(T* ptr, [[maybe_unused]] size_t n) {
    assert(reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0);
    if constexpr (alignof(T) > internal::pool_allocator::max_alignment) {
      p_free(static_cast<void*>(ptr));
    }
    else {
      internal::get_default_allocator().deallocate(static_cast<void*>(ptr), n * sizeof(T));
    }
  }

  constexpr allocator() { internal::get_default_allocator(); };
  template <class U> /* implicit */ constexpr allocator(const allocator<U>&) noexcept { }
};

template<typename T>
struct is_trivially_relocatable<allocator<T>> : std::true_type {};

template <class T, class U>
bool operator==(const allocator<T>&, const allocator<U>&) { return true; }
template <class T, class U>
bool operator!=(const allocator<T>&, const allocator<U>&) { return false; }


// ----------------------------------------------------------------------------
// Static allocator for single items of a given type, e.g.
//   using long_allocator = type_allocator<long>;
//   long* foo = long_allocator::alloc();
//   new (foo) long{23};
//   long_allocator::free(foo);
//
// Uses block allocator internally, and is headerless
// ----------------------------------------------------------------------------

namespace internal {
template<size_t Size, size_t Align>
extern inline block_allocator& get_block_allocator() {
  static block_allocator a(Size, Align);
  return a;
}
}

// A static allocator for allocating storage for single objects of a fixed
// type. Much more efficient than using p_malloc or parlay::allocator for
// individual objects.
//
// Can be used to allocate raw uninitialized storage via alloc()/free(ptr),
// or to perform combined allocation and construction with create(args...)
// followed by destroy(ptr) to destroy and deallocate.
//
// alloc() -> T*         : returns uninitialized storage for an object of type T
// free(T*)              : deallocates storage obtained by alloc
//
// create(args...) -> T* : allocates storage for and constructs a T using args...
// destroy(T*)           : destroys and deallocates a T obtained from create(...)
//
// All members are static, so it is not required to create an instance of
// type_allocator<T> to use it.
//
template <typename T>
class type_allocator {

  static internal::block_allocator& get_allocator() {
    return internal::get_block_allocator<sizeof(T), alignof(T)>();
  }

public:

  // Allocate uninitialized storage appropriate for storing an object of type T
  static T* alloc() {
    void* buffer = get_allocator().alloc();
    assert(reinterpret_cast<uintptr_t>(buffer) % alignof(T) == 0);
    return static_cast<T*>(buffer);
  }

  // Free storage obtained by alloc()
  static void free(T* ptr) {
    assert(ptr != nullptr);
    assert(reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0);
    get_allocator().free(static_cast<void*>(ptr));
  }

  // Allocate storage for and then construct an object of type T using args...
  template<typename... Args>
  static T* create(Args... args) {
    static_assert(std::is_constructible_v<T, Args...>);
    return new (alloc()) T(std::forward<Args>(args)...);
  }

  // Destroy an object obtained by create(...) and deallocate its storage
  static void destroy(T* ptr) {
    assert(ptr != nullptr);
    ptr->~T();
    free(ptr);
  }

  // for backward compatibility -----------------------------------------------
  static constexpr inline size_t default_alloc_size = 0;
  static constexpr inline bool initialized = true;

  template <typename ... Args>
  static T* allocate(Args... args) { return create(std::forward<Args>(args)...); }
  static void retire(T* ptr) { destroy(ptr); }
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
