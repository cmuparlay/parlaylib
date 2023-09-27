#pragma once

#include <cassert>
#include <cstddef>

namespace parlay {

template<typename T>
class marked_ptr {

  static constexpr uintptr_t ONE_BIT = 1;
  static constexpr uintptr_t TWO_BIT = 1 << 1;

 public:
  marked_ptr() : ptr(0) {}

  /* implicit */ marked_ptr(std::nullptr_t) : ptr(0) {}                 // NOLINT(google-explicit-constructor)

  /* implicit */ marked_ptr(T *new_ptr) : ptr(reinterpret_cast<uintptr_t>(new_ptr)) {}      // NOLINT(google-explicit-constructor)

  /* implicit */ operator T* () const { return get_ptr(); }             // NOLINT(google-explicit-constructor)

  typename std::add_lvalue_reference_t<T> operator*() const { return *(get_ptr()); }

  T* operator->() { return get_ptr(); }

  const T *operator->() const { return get_ptr(); }

  bool operator==(const marked_ptr &other) const { return ptr == other.ptr; }

  bool operator!=(const marked_ptr &other) const { return ptr != other.ptr; }

  bool operator==(const T *other) const { return get_ptr() == other; }

  bool operator!=(const T *other) const { return get_ptr() != other; }

  operator bool() const noexcept { return get_ptr() != nullptr; }         // NOLINT(google-explicit-constructor)

  T* get_ptr() const { return reinterpret_cast<T*>(ptr & ~(ONE_BIT | TWO_BIT)); }

  void set_ptr(T* new_ptr) { ptr = reinterpret_cast<uintptr_t>(new_ptr) | get_mark(); }

  [[nodiscard]] uintptr_t get_mark() const { return ptr & (ONE_BIT | TWO_BIT); }

  marked_ptr& clear_mark() {
    ptr = ptr & ~(ONE_BIT | TWO_BIT);
    return *this;
  }

  marked_ptr& set_mark(uintptr_t mark) {
    assert(mark < (1 << 2));  // Marks should only occupy the bottom two bits
    clear_mark();
    ptr |= mark;
    return *this;
  }

 private:
  uintptr_t ptr;
};

}  // namespace parlay

