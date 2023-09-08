
#ifndef PARLAY_WORKER_SPECIFIC_H_
#define PARLAY_WORKER_SPECIFIC_H_

#include <cassert>
#include <cstddef>

#include <iterator>
#include <type_traits>
#include <utility>

#include "parallel.h"
#include "range.h"
#include "type_traits.h"
#include "utilities.h"

#if !defined(NDEBUG) && defined(PARLAY_USING_PARLAY_SCHEDULER)
#define PARLAY_DEBUG_WORKER_SPECIFIC
#endif

#if defined(PARLAY_DEBUG_WORKER_SPECIFIC)
#define PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER assert(&internal::get_current_scheduler() == owning_scheduler &&  \
  "parlay::WorkerSpecific<> must ONLY be used within the scheduler instance in which it was created.");
#else
#define PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER
#endif

namespace parlay {

template<typename T>
class WorkerSpecific {

  struct alignas(64) data {
    // We take the value wrapped in a function call so that we can instantiate
    // uncopyable types from prvalues using copy elision.  Otherwise, it would
    // not be possible to initialize uncopyable types (e.g., std::atomic)
    template<typename F>
    explicit data(F&& f) : x(f()) { }

    data(const data&) = delete;
    data(data&&) = delete;

    T x;
  };

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
  explicit WorkerSpecific(F&& f) : elements(make_unique_array<data>(num_workers(),
      [f = std::forward<F>(f)](std::size_t i) { return [&f, i]() { return f(i); }; }))
#if defined(PARLAY_DEBUG_WORKER_SPECIFIC)
      , owning_scheduler(std::addressof(internal::get_current_scheduler()))
#endif
  { }

  WorkerSpecific(const WorkerSpecific&) = delete;
  WorkerSpecific& operator=(const WorkerSpecific&) = delete;
  WorkerSpecific(WorkerSpecific&&) = delete;

  T& operator*() { return get(); }
  T* operator->() { return std::addressof(get()); }
  [[nodiscard]] T& get() {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return elements[worker_id()].x;
  }

  const T& operator*() const { return get(); }
  T const* operator->() const { return std::addressof(get()); }
  [[nodiscard]] const T& get() const {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return elements[worker_id()].x;
  }

  template<typename F>
  void for_each(F&& f) {
    std::for_each(begin(), end(), std::forward<F>(f));
  }

  // Allow looping over all thread's data
  template<bool Const>
  class iterator_t {
    friend class WorkerSpecific<T>;

    explicit iterator_t(data* ptr_) : ptr(ptr_) { }

   public:
    using iterator_category = std::random_access_iterator_tag;
    using reference = std::add_lvalue_reference_t<maybe_const_t<Const, T>>;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = std::add_pointer_t<maybe_const_t<Const, T>>;

    iterator_t() : ptr(nullptr) { }

    /* implicit */ iterator_t(const iterator_t<false>& other) : ptr(other.ptr) { }  // cppcheck-suppress noExplicitConstructor    // NOLINT

    reference operator*() const { return ptr->x; }
    reference operator[](std::size_t p) const { return ptr[p].x; }

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
    maybe_const_t<Const, data>* ptr;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  static_assert(is_random_access_iterator_v<iterator>);
  static_assert(is_random_access_iterator_v<const_iterator>);

  [[nodiscard]] iterator begin() {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return iterator{&elements[0]};
  }

  [[nodiscard]] iterator end() {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return iterator{&elements[0] + num_workers()};
  }

  [[nodiscard]] const_iterator begin() const {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return const_iterator{&elements[0]};
  }

  [[nodiscard]] const_iterator end() const {
    PARLAY_WORKER_SPECIFIC_CHECK_SCHEDULER;
    return const_iterator{&elements[0] + num_workers()};
  }

 private:
  unique_array<data> elements;

#if defined(PARLAY_DEBUG_WORKER_SPECIFIC)
  // In DEBUG mode only, remember the scheduler instance that this belongs to
  internal::scheduler_type* owning_scheduler;
#endif
};

static_assert(is_random_access_range_v<WorkerSpecific<int>>);
static_assert(is_random_access_range_v<const WorkerSpecific<int>>);

}


#endif  // PARLAY_WORKER_SPECIFIC_H_
