
#ifndef PARLAY_THREAD_SPECIFIC_H_
#define PARLAY_THREAD_SPECIFIC_H_

#include <cassert>
#include <cstddef>

#include <array>
#include <algorithm>      // IWYU pragma: keep
#include <atomic>
#include <functional>
#include <iterator>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "internal/thread_id_pool.h"

#include "portability.h"
#include "range.h"
#include "type_traits.h"


namespace parlay {

using internal::thread_id_type;

// Returns a unique thread ID for the current running thread
// in the range of [0...num_thread_ids()).  Thread IDs are
// guaranteed to be unique for all *live* threads, but they
// are re-used after a thread dies and another is spawned.
inline thread_id_type my_thread_id() {
  return internal::get_thread_id();
}

// Return the number of thread IDs that have been assigned to
// threads.  All thread IDs are in the range [0...num_thread_ids()).
//
// Important note:  Thread IDs are assigned lazily when a thread
// first requests one.  Therefore num_thread_ids() is *not*
// guaranteed to be as large as the number of live threads if
// those threads have never called my_thread_id().
inline thread_id_type num_thread_ids() {
  return internal::get_num_thread_ids();
}

namespace internal {

class ThreadListChunkData {

 public:

  // This is just std::bit_ceil(std::thread::hardware_concurrency()) but we don't assume C++20
  const static inline std::size_t thread_list_chunk_size = []() {
    std::size_t size = 4;
    while (size < std::thread::hardware_concurrency())
      size *= 2;
    return size;
  }();

  // Used by ThreadSpecific which stores a chunked sequence of items that is at least as large
  // as the number of active threads.  Given a thread ID, items are split into chunks of size:
  //
  //   P, P, 2P, 4P, 8P, ...
  //
  // where P is the lowest power of two that is at least as large as the number of hardware threads.
  static std::size_t compute_chunk_id(thread_id_type id) {
    std::size_t k = thread_list_chunk_size;
    std::size_t chunk = 0;
    while (k <= id) {
      chunk++;
      k *= 2;
    }
    return chunk;
  }

  static std::size_t compute_chunk_position(thread_id_type id, std::size_t chunk_id) {
    if (chunk_id == 0)
      return id;
    else {
      auto high_bit = thread_list_chunk_size << (chunk_id - 1);
      assert(id & high_bit);
      return id - high_bit;
    }
  }

  explicit ThreadListChunkData(thread_id_type thread_id_) noexcept : thread_id(thread_id_),
      chunk_id(compute_chunk_id(thread_id)), chunk_position(compute_chunk_position(thread_id, chunk_id)) { }

  const thread_id_type thread_id;
  const std::size_t chunk_id;
  const std::size_t chunk_position;
};

extern inline const ThreadListChunkData& get_chunk_data() {
  static thread_local const ThreadListChunkData data{get_thread_id()};
  return data;
}

template<typename T>
struct Uninitialized {
  union {
    alignas(64) std::monostate empty;
    T value;
  };

  Uninitialized() noexcept { };

  T& operator*() { return value; }

  T* get() { return std::addressof(value); }

  ~Uninitialized() { value.~T(); }
};

}  // namespace internal

// A ThreadSpecific<T> stores a list of objects of type T such that there
// is a unique object for each active thread. The list automatically grows
// when additional threads are spawned and attempt to access it. Threads
// may also traverse the entire list if they need to.
//
// By default, list elements are all value initialized, roughly meaning
// that class types are default constructed, and builtin types are zero
// initialized.  For custom initialization, you can pass a constructor
// function which returns the desired value.  The constructor function
// can take zero or one arguments.  If it takes one argument, it will be
// passed the thread ID that it is constructing for.  Note that the
// elements are not guaranteed to be constructed by the thread that
// they belong to, and they may be constructed in advance of any thread
// actually taking ownership of that ID.
//
// A few things to note:
//
// - Thread IDs are always unique for the set of currently live threads,
//   but not unique over the course of the entire program.  A thread that
//   dies will give up its ID to be claimed by a new thread later.
//
// - The list elements are *not* destroyed when the thread that "owns"
//   them is destroyed. A new thread that reclaims a previously-used ID
//   will find the item at that position in the same state that it was
//   left by the previous thread.  Elements are only destroyed when the
//   entire ThreadSpecific is destroyed.
//
//   Therefore, threads are responsible for manually cleaning up the
//   contents of a ThreadSpecific and/or resetting it to a default value
//   for the next thread that might claim the spot if they need to.
//
template<typename T>
class ThreadSpecific {

  // 25 chunks guarantees enough slots for any machine
  // with up to 2^48 bytes of addressable virtual memory,
  // assuming that threads are 8MB large.
  static constexpr std::size_t n_chunks = 25;

 public:

  using reference = T&;
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;

  ThreadSpecific() : constructor([](std::size_t) { return T{}; }) {
    initialize();
  }

  template<typename F,
      typename std::enable_if_t<std::is_invocable_v<F&> && !std::is_invocable_v<F&, std::size_t>, int> = 0>
  explicit ThreadSpecific(F&& constructor_)
      : constructor([f = std::forward<F>(constructor_)](std::size_t) { return f(); }) {
    initialize();
  }

  template<typename F,
      typename std::enable_if_t<std::is_invocable_v<F&, std::size_t>, int> = 0>
  explicit ThreadSpecific(F&& constructor_) : constructor(std::forward<F>(constructor_)) {
    initialize();
  }

  ThreadSpecific(const ThreadSpecific&) = delete;
  ThreadSpecific& operator=(const ThreadSpecific&) = delete;
  ThreadSpecific(ThreadSpecific&&) = delete;

  ~ThreadSpecific() {
    for (internal::Uninitialized<T>* chunk : chunks) {
      delete[] chunk;
    }
  }

  T& operator*() { return get(); }
  T* operator->() { return std::addressof(get()); }

  T& get() {
    auto chunk_data = internal::get_chunk_data();
    return get_by_index(chunk_data.chunk_id, chunk_data.chunk_position);
  }

  const T& operator*() const { return get(); }
  T const* operator->() const { return std::addressof(get()); }

  const T& get() const {
    auto chunk_data = internal::get_chunk_data();
    return get_by_index(chunk_data.chunk_id, chunk_data.chunk_position);
  }

  template<typename F>
  void for_each(F&& f) {
    static_assert(std::is_invocable_v<F, T&>);

    auto num_threads = num_thread_ids();
    thread_id_type tid = 0;
    internal::Uninitialized<T>* chunk = chunks[0].load(std::memory_order_relaxed);

    for (std::size_t chunk_id = 0; tid < num_threads; chunk = chunks[++chunk_id].load(std::memory_order_acquire)) {
      auto chunk_size = get_chunk_size(chunk_id);
      if (!chunk) PARLAY_UNLIKELY {
        ensure_chunk_exists(chunk_id);
        chunk = chunks[chunk_id].load(std::memory_order_relaxed);
      }
      for (std::size_t i = 0; tid < num_threads && i < chunk_size; i++, tid++) {
        f(*chunk[i]);
      }
    }
  }

  // Allow looping over all thread's data
  template<bool Const>
  class iterator_t {
    friend class ThreadSpecific<T>;

    using parent_type = maybe_const_t<Const, ThreadSpecific<T>>;

    iterator_t(std::size_t chunk_id_, std::size_t position_, parent_type* parent_) :
        chunk_id(chunk_id_), position(position_), parent(parent_) { }

   public:
    using iterator_category = std::random_access_iterator_tag;
    using reference = std::add_lvalue_reference_t<maybe_const_t<Const, T>>;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = std::add_pointer_t<maybe_const_t<Const, T>>;

    iterator_t() = default;

    /* implicit */ iterator_t(const iterator_t<false>& other)  // cppcheck-suppress noExplicitConstructor    // NOLINT
        : chunk_id(other.chunk_id), position(other.position), parent(other.parent) { }

    reference operator*() const { return parent->get_by_index_nocheck(chunk_id, position); }

    reference operator[](std::size_t p) const {
      auto tmp = *this;
      tmp += p;
      return *tmp;
    }

    iterator_t& operator++() {
      position++;
      if (position == get_chunk_size(chunk_id)) {
        if (++chunk_id < n_chunks && parent->chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
          parent->ensure_chunk_exists(chunk_id);
        position = 0;
      }
      return *this;
    }

    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }   //NOLINT

    iterator_t& operator--() {
      if (position == 0) {
        position = get_chunk_size(--chunk_id) - 1;
        if (parent->chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
          parent->ensure_chunk_exists(chunk_id);
      }
      else {
        position--;
      }
      return *this;
    }

    iterator_t operator--(int) { auto tmp = *this; --(*this); return tmp; }   //NOLINT

    iterator_t& operator+=(difference_type diff) {
      if (diff < 0) return *this -= (-diff);
      assert(diff >= 0);
      position += diff;
      if (position >= get_chunk_size(chunk_id)) {
        do {
          position -= get_chunk_size(chunk_id++);
        } while (position >= get_chunk_size(chunk_id));
        if (parent->chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
          parent->ensure_chunk_exists(chunk_id);
      }
      return *this;
    }

    iterator_t operator+(difference_type diff) const {
      auto result = *this;
      result += diff;
      return result;
    }

    iterator_t& operator-=(difference_type diff) {
      if (diff < 0) return *this += (-diff);
      assert(diff >= 0);
      auto pos = static_cast<difference_type>(position);
      pos -= diff;
      if (pos < 0) {
        do {
          pos += static_cast<difference_type>(get_chunk_size(--chunk_id));
        } while (pos < 0);
        if (parent->chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
          parent->ensure_chunk_exists(chunk_id);
      }
      assert(pos >= 0);
      position = static_cast<std::size_t>(pos);
      return *this;
    }

    iterator_t operator-(difference_type diff) const {
      auto result = *this;
      result -= diff;
      return result;
    }

    difference_type operator-(const iterator_t& other) const {
      if (other > *this) return -(other - *this);
      assert(other <= *this);
      auto result = static_cast<difference_type>(position) - static_cast<difference_type>(other.position);
      auto chunk_id_ = other.chunk_id;
      while (chunk_id_ < chunk_id) {
        result += static_cast<difference_type>(get_chunk_size(chunk_id_++));
      }
      return result;
    }

    bool operator==(const iterator_t& other) const {
      return chunk_id == other.chunk_id && position == other.position;
    }

    bool operator!=(const iterator_t& other) const {
      return chunk_id != other.chunk_id || position != other.position;
    }

    bool operator<(const iterator_t& other) const {
      return chunk_id < other.chunk_id || (chunk_id == other.chunk_id && position < other.position);
    }

    bool operator<=(const iterator_t& other) const {
      return chunk_id < other.chunk_id || (chunk_id == other.chunk_id && position <= other.position);
    }

    bool operator>(const iterator_t& other) const {
      return chunk_id > other.chunk_id || (chunk_id == other.chunk_id && position > other.position);
    }

    bool operator>=(const iterator_t& other) const {
      return chunk_id > other.chunk_id || (chunk_id == other.chunk_id && position >= other.position);
    }

    friend void swap(iterator_t& left, iterator_t& right) {
      std::swap(left.chunk_id, right.chunk_id);
      std::swap(left.position, right.position);
      std::swap(left.parent, right.parent);
    }

    std::size_t chunk_id{n_chunks};
    std::size_t position{0};
    parent_type* parent{nullptr};
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  static_assert(is_random_access_iterator_v<iterator>);
  static_assert(is_random_access_iterator_v<const_iterator>);

  [[nodiscard]] iterator begin() {
    return iterator{0,0,this};
  }

  [[nodiscard]] const_iterator begin() const {
    return const_iterator{0,0,this};
  }

  [[nodiscard]] iterator end() {
    internal::ThreadListChunkData data{num_thread_ids()};
    return iterator{data.chunk_id, data.chunk_position, this};
  }

  [[nodiscard]] const_iterator end() const {
    internal::ThreadListChunkData data{num_thread_ids()};
    return const_iterator{data.chunk_id, data.chunk_position, this};
  }

 private:

  void initialize() {
    internal::get_chunk_data();  //  Force static initialization before any ThreadLocals are constructed
    chunks[0].store(new internal::Uninitialized<T>[internal::ThreadListChunkData::thread_list_chunk_size], std::memory_order_relaxed);
    std::fill(chunks.begin() + 1, chunks.end(), nullptr);
    auto chunk = chunks[0].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < internal::ThreadListChunkData::thread_list_chunk_size; i++) {
      new (static_cast<void*>(chunk[i].get())) T(constructor(i));
    }
  }

  static std::size_t get_chunk_size(std::size_t chunk_id) {
    assert(chunk_id < n_chunks);
    if (chunk_id == 0) return internal::ThreadListChunkData::thread_list_chunk_size;
    else return internal::ThreadListChunkData::thread_list_chunk_size << (chunk_id - 1);
  }

  T& get_by_index(std::size_t chunk_id, std::size_t chunk_position) {
    if (chunk_id > 0 && chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
      ensure_chunk_exists(chunk_id);
    return get_by_index_nocheck(chunk_id, chunk_position);
  }

  const T& get_by_index(std::size_t chunk_id, std::size_t chunk_position) const {
    if (chunk_id > 0 && chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
    ensure_chunk_exists(chunk_id);
    return get_by_index_nocheck(chunk_id, chunk_position);
  }

  T& get_by_index_nocheck(std::size_t chunk_id, std::size_t chunk_position) {
    assert(chunks[chunk_id].load() != nullptr);
    return *(chunks[chunk_id].load(std::memory_order_relaxed)[chunk_position]);
  }

  const T& get_by_index_nocheck(std::size_t chunk_id, std::size_t chunk_position) const {
    assert(chunks[chunk_id].load() != nullptr);
    return *(chunks[chunk_id].load(std::memory_order_relaxed)[chunk_position]);
  }

  void ensure_chunk_exists(std::size_t chunk_id) const {
    std::lock_guard<std::mutex> lock(growing_mutex);
    if (chunks[chunk_id].load(std::memory_order_relaxed) == nullptr) {
      auto chunk_size = get_chunk_size(chunk_id);
      auto chunk = new internal::Uninitialized<T>[chunk_size];
      for (std::size_t i = 0; i < chunk_size; i++) {
        new (static_cast<void*>(chunk[i].get())) T(constructor(chunk_size + i));
      }
      chunks[chunk_id].store(chunk, std::memory_order_release);
    }
  }

  mutable std::function<T(thread_id_type)> constructor;
  mutable std::mutex growing_mutex;
  mutable std::array<std::atomic<internal::Uninitialized<T>*>, n_chunks> chunks;
};

static_assert(is_random_access_range_v<ThreadSpecific<int>>);
static_assert(is_random_access_range_v<const ThreadSpecific<int>>);

}  // namespace parlay


#endif  // PARLAY_THREAD_SPECIFIC_H_
