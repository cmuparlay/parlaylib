
#ifndef PARLAY_WORKER_SPECIFIC_H_
#define PARLAY_WORKER_SPECIFIC_H_

#include <cstddef>

#include <type_traits>
#include <utility>

#include "parallel.h"
#include "range.h"
#include "utilities.h"

namespace parlay {

template<typename T>
class WorkerSpecific {

  using rep_type = padded<T, 64>;

 public:

  using reference = T&;
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;

  WorkerSpecific() : WorkerSpecific([](std::size_t) { return T{}; }) { }

  template<typename F,
    std::enable_if_t<std::is_invocable_v<F&> && !std::is_invocable_v<F&, std::size_t>, int> = 0>
  explicit WorkerSpecific(F&& f) : WorkerSpecific([f = std::forward<F>(f)](std::size_t) { return f(); }) { }

  template<typename F,
    std::enable_if_t<std::is_invocable_v<F&, std::size_t>, int> = 0>
  explicit WorkerSpecific(F&& f) : elements(make_unique_array<rep_type>(num_workers(), std::forward<F>(f))) { }

  WorkerSpecific(const WorkerSpecific&) = delete;
  WorkerSpecific& operator=(const WorkerSpecific&) = delete;
  WorkerSpecific(WorkerSpecific&&) = delete;

  T& operator*() { return get(); }
  T* operator->() { return std::addressof(get()); }
  T& get() { return elements[worker_id()]; }

  const T& operator*() const { return get(); }
  T const* operator->() const { return std::addressof(get()); }
  const T& get() const { return elements[worker_id()]; }

  template<typename F>
  void for_each(F&& f) {
    std::for_each(begin(), end(), std::forward<F>(f));
  }

  // Allow looping over all thread's data
  template<bool Const>
  class iterator_t {
    friend class WorkerSpecific<T>;

    explicit iterator_t(rep_type* ptr_) : ptr(ptr_) { }

   public:
    using iterator_category = std::random_access_iterator_tag;
    using reference = std::add_lvalue_reference_t<maybe_const_t<Const, T>>;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = std::add_pointer_t<maybe_const_t<Const, T>>;

    iterator_t() = default;

    /* implicit */ iterator_t(const iterator_t<false>& other) : ptr(other.ptr) { }  // cppcheck-suppress noExplicitConstructor    // NOLINT

    reference operator*() const { return *ptr; }
    reference operator[](std::size_t p) const { return ptr[p]; }

    iterator_t& operator++() { ptr++; return *this; }
    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }   //NOLINT
    iterator_t& operator--() { ptr--; return *this; }
    iterator_t operator--(int) { auto tmp = *this; --(*this); return tmp; }   //NOLINT

    iterator_t& operator+=(difference_type diff) { ptr += diff; return *this; }
    iterator_t& operator-=(difference_type diff) { ptr -= diff; return *this; }

    iterator_t operator+(difference_type diff) const { return iterator_t{ptr + diff}; }
    iterator_t operator-(difference_type diff) const { return iterator_t{ptr - diff}; }

    difference_type operator-(const iterator_t& other) const { return ptr - other.ptr; }

    bool operator==(const iterator_t& other) const { return ptr == other.ptr; }
    bool operator!=(const iterator_t& other) const { return ptr != other.ptr; }
    bool operator<(const iterator_t& other) const { return ptr < other.ptr; }
    bool operator<=(const iterator_t& other) const { return ptr <= other.ptr; }
    bool operator>(const iterator_t& other) const { return ptr > other.ptr; }
    bool operator>=(const iterator_t& other) const { return ptr >= other.ptr; }

    friend void swap(iterator_t& left, iterator_t& right) { std::swap(left.ptr, right.ptr); }

   private:
    maybe_const_t<Const, rep_type>* ptr;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  static_assert(is_random_access_iterator_v<iterator>);
  static_assert(is_random_access_iterator_v<const_iterator>);

  iterator begin() {
    return iterator{&elements[0]};
  }

  iterator end() {
    return iterator{&elements[0] + num_workers()};
  }

  const_iterator begin() const {
    return const_iterator{&elements[0]};
  }

  const_iterator end() const {
    return const_iterator{&elements[0] + num_workers()};
  }

 private:
  unique_array<padded<T, 64>> elements;
};

static_assert(is_random_access_range_v<WorkerSpecific<int>>);
static_assert(is_random_access_range_v<const WorkerSpecific<int>>);

}


#endif  // PARLAY_WORKER_SPECIFIC_H_
