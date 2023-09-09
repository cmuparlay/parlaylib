
#ifndef PARLAY_INTERNAL_UNINITIALIZED_SEQUENCE_H_
#define PARLAY_INTERNAL_UNINITIALIZED_SEQUENCE_H_

#include <iterator>
#include <memory>

#include "../alloc.h"
#include "../portability.h"

#include "debug_uninitialized.h"

namespace parlay {
namespace internal {
  
#ifndef PARLAY_USE_STD_ALLOC
template<typename T>
using _uninitialized_sequence_default_allocator = parlay::allocator<T>;
#else
template<typename T>
using _uninitialized_sequence_default_allocator = std::allocator<T>;
#endif

// An uninitialized fixed-size sequence container.
//
// By uninitialized, we mean two things:
//  - The constructor uninitialized_sequence<T>(n) does not initialize its elements
//  - The destructor ~uninitialized_sequence() also DOES NOT destroy its elements
//
// In other words, the elements of the sequence are uninitialized upon construction,
// and are also required to be uninitialized when the sequence is destroyed.
//
// Q: What on earth is the purpose of such a container??
// A: Its purpose is to be used as temporary storage for out of place algorithms
//    that use uninitialized_relocate. Since the container begins in an uninitialized
//    state, it is valid to uninitialized_relocate objects into it, and then
//    uninitialized_relocate them back out. This leaves the elements uninitialized,
//    so that no destructors will accidentally be triggered for moved-out-of objects.
//
template<typename T, typename Alloc = _uninitialized_sequence_default_allocator<T>>
class uninitialized_sequence {
 public:
  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using pointer = T*;
  using const_pointer = const T*;
  
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using allocator_type = Alloc;
  
 private: 
  struct uninitialized_sequence_impl : public allocator_type {
    size_t n;
    value_type* data;
    explicit uninitialized_sequence_impl(size_t _n, const allocator_type& alloc)
             : allocator_type(alloc),
               n(_n),
               data(std::allocator_traits<allocator_type>::allocate(*this, n)) { }
    ~uninitialized_sequence_impl() {
      std::allocator_traits<allocator_type>::deallocate(*this, data, n);
    }

    // Delete copy for uninitialized sequences
    uninitialized_sequence_impl(const uninitialized_sequence_impl&) = delete;
    uninitialized_sequence_impl& operator=(const uninitialized_sequence_impl&) = delete;

    // Move constructing an uninitialized sequence
    // leaves the other sequence in an empty state
    uninitialized_sequence_impl(uninitialized_sequence_impl&& other) noexcept : data(other.data), n(other.n) {
      other.data = nullptr;
      other.n = 0;
    }

    // Move assigning an uninitialized sequence clears the current sequence
    // (without destruction) and swaps it with the given other sequence
    uninitialized_sequence_impl& operator=(uninitialized_sequence_impl&& other) noexcept {
#ifdef PARLAY_DEBUG_UNINITIALIZED
      // If uninitialized memory debugging is turned on, make sure that
      // each object of type UninitializedTracker is destroyed or still
      // uninitialized by the time this sequence is destroyed
      auto buffer = impl.data;
      parallel_for(0, impl.n, [&](size_t i) {
        PARLAY_ASSERT_UNINITIALIZED(buffer[i]);
      });
#endif
      n = 0;
      data = nullptr;
      std::swap(n, other.n);
      std::swap(data, other.data);
    }

  } impl;

 public:
  explicit uninitialized_sequence(size_t n, const allocator_type& alloc = {})
           : impl(n, alloc) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
    // If uninitialized memory debugging is turned on, make sure that
    // each object of type UninitializedTracker is appropriately set
    // to its uninitialized state.
    if constexpr (std::is_same_v<value_type, UninitializedTracker>) {
      auto buffer = impl.data;
      parallel_for(0, n, [&](size_t i) {
        buffer[i].initialized = false;
      });
    }
#endif
  }

  uninitialized_sequence(const uninitialized_sequence<T, Alloc>&) = delete;
  uninitialized_sequence& operator=(const uninitialized_sequence<T, Alloc>&) = delete;

  uninitialized_sequence(uninitialized_sequence<T, Alloc>&&) noexcept = default;
  uninitialized_sequence& operator=(uninitialized_sequence<T, Alloc>&&) noexcept = default;

#ifdef PARLAY_DEBUG_UNINITIALIZED
  // If uninitialized memory debugging is turned on, make sure that
  // each object of type UninitializedTracker is destroyed or still
  // uninitialized by the time this sequence is destroyed
  ~uninitialized_sequence() {
    auto buffer = impl.data;
    parallel_for(0, impl.n, [&](size_t i) {
      PARLAY_ASSERT_UNINITIALIZED(buffer[i]);
    });
  }
#else
  ~uninitialized_sequence() = default;
#endif

  iterator begin() { return impl.data; }
  iterator end() { return impl.data + impl.n; }

  const_iterator begin() const { return impl.data; }
  const_iterator end() const { return impl.data + impl.n; }

  const_iterator cbegin() const { return impl.data; }
  const_iterator cend() const { return impl.data + impl.n; }
  
  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
  
  const_reverse_iterator rbegin() const { return std::make_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return std::make_reverse_iterator(begin()); }
  
  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }
  
  void swap(uninitialized_sequence<T, Alloc>& other) {
    std::swap(impl.n, other.impl.n);
    std::swap(impl.data, other.impl.data);
  }
  
  [[nodiscard]] size_type size() const { return impl.n; }
  
  value_type* data() { return impl.data; }
  
  const value_type* data() const { return impl.data; }

  value_type& operator[](size_t i) { return impl.data[i]; }
  const value_type& operator[](size_t i) const { return impl.data[i]; }
  
  value_type& at(size_t i) {
    if (i >= size()) {
      throw_exception_or_terminate<std::out_of_range>("uninitialized_sequence access out of bounds: length = " +
                                                      std::to_string(size()) + ", index = " + std::to_string(i));
    }
    else {
      return impl.data[i];
    }
  }
  
  const value_type& at(size_t i) const {
    if (i >= size()) {
      throw_exception_or_terminate<std::out_of_range>("uninitialized_sequence access out of bounds: length = " +
                                                      std::to_string(size()) + ", index = " + std::to_string(i));
    }
    else {
      return impl.data[i];
    }
  }
};  
  
}  // namespace internal  
}  // namespace parlay

#endif  // PARLAY_INTERNAL_UNINITIALIZED_SEQUENCE_H_
