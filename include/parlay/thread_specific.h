
#ifndef PARLAY_THREAD_LOCAL_H_
#define PARLAY_THREAD_LOCAL_H_

#include <cstddef>

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <variant>

#include "internal/thread_id_pool.h"

#include "portability.h"

namespace parlay {

// Returns a unique thread ID for the current running thread
// in the range of [0...num_thread_ids()).  Thread IDs are
// guaranteed to be unique for all *live* threads, but they
// are re-used after a thread dies and another is spawned.
inline std::size_t my_thread_id() {
  return internal::get_thread_id();
}

// Return the number of thread IDs that have been assigned to
// threads.  All thread IDs are in the range [0...num_thread_ids()).
//
// Important note:  Thread IDs are assigned lazily when a thread
// first requests one.  Therefore num_thread_ids() is *not*
// guaranteed to be as large as the number of live threads if
// those threads have never called my_thread_id().
inline std::size_t num_thread_ids() {
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

 private:

  // Used by ThreadSpecific which stores a chunked sequence of items that is at least as large
  // as the number of active threads.  Given a thread ID, items are split into chunks of size:
  //
  //   P, P, 2P, 4P, 8P, ...
  //
  // where P is the lowest power of two that is at least as large as the number of hardware threads.
  static std::size_t compute_chunk_id(std::size_t id) {
    std::size_t k = thread_list_chunk_size;
    std::size_t chunk = 0;
    while (k <= id) {
      chunk++;
      k *= 2;
    }
    return chunk;
  }

  static std::size_t compute_chunk_position(std::size_t id, std::size_t chunk_id) {
    if (chunk_id == 0)
      return id;
    else {
      auto high_bit = thread_list_chunk_size << (chunk_id - 1);
      assert(id & high_bit);
      return id - high_bit;
    }
  }

  explicit ThreadListChunkData(std::size_t thread_id_) noexcept : thread_id(thread_id_),
      chunk_id(compute_chunk_id(thread_id)), chunk_position(compute_chunk_position(thread_id, chunk_id)) { }

 public:
  friend inline const ThreadListChunkData& get_chunk_data();

  const std::size_t thread_id;
  const std::size_t chunk_id;
  const std::size_t chunk_position;
};

inline const ThreadListChunkData& get_chunk_data() {
  static thread_local const ThreadListChunkData data{get_thread_id()};
  return data;
}

template<typename T>
struct alignas(64) Uninitialized {
  union {
    std::monostate empty;
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
// function which must construct the value in place, i.e., it should look
// something like
//
//  [](void* p) { new (p) T{...}; }
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
template<typename T>
class ThreadSpecific {

 public:

  ThreadSpecific() : constructor([](void* p) { new (p) T{}; }) {
    initialize();
  }

  template<typename F>
  explicit ThreadSpecific(F&& constructor_) : constructor(std::forward<F>(constructor_)) {
    initialize();
  }

  ThreadSpecific(const ThreadSpecific&) = delete;
  ThreadSpecific& operator=(const ThreadSpecific&) = delete;

  ~ThreadSpecific() {
    for (internal::Uninitialized<T>* chunk : chunks) {
      if (chunk) {
        delete[] chunk;
      }
    }
  }

  T& operator*() { return get(); }

  T* operator->() { return std::addressof(get()); }

  T& get() {
    auto chunk_data = internal::get_chunk_data();
    return get_by_index(chunk_data.chunk_id, chunk_data.chunk_position);
  }

  template<typename F>
  void for_each(F&& f) {
    static_assert(std::is_invocable_v<F, T&>);

    auto num_threads = num_thread_ids();
    std::size_t tid = 0;
    internal::Uninitialized<T>* chunk = chunks[0].load(std::memory_order_relaxed);

    for (std::size_t j = 0; chunk != nullptr; chunk = chunks[++j].load(std::memory_order_acquire)) {
      auto chunk_size = get_chunk_size(j);
      for (std::size_t i = 0; tid < num_threads && i < chunk_size; i++, tid++) {
        f(*chunk[i]);
      }
    }
  }

 private:

  void initialize() {
    chunks[0].store(new internal::Uninitialized<T>[internal::ThreadListChunkData::thread_list_chunk_size], std::memory_order_relaxed);
    std::fill(chunks.begin() + 1, chunks.end(), nullptr);
    auto chunk = chunks[0].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < internal::ThreadListChunkData::thread_list_chunk_size; i++) {
      constructor(static_cast<void*>(chunk[i].get()));
    }
  }

  std::size_t get_chunk_size(std::size_t chunk_id) {
    if (chunk_id == 0) return internal::ThreadListChunkData::thread_list_chunk_size;
    else return internal::ThreadListChunkData::thread_list_chunk_size << (chunk_id - 1);
  }

  T& get_by_index(std::size_t chunk_id, std::size_t chunk_position) {
    if (chunk_id > 0 && chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
      ensure_chunk_exists(chunk_id);
    assert(chunks[chunk_id].load() != nullptr);
    return *(chunks[chunk_id].load(std::memory_order_relaxed)[chunk_position]);
  }

  void ensure_chunk_exists(std::size_t chunk_id) {
    std::lock_guard<std::mutex> lock(growing_mutex);
    if (chunks[chunk_id].load(std::memory_order_relaxed) == nullptr) {
      auto chunk_size = get_chunk_size(chunk_id);
      auto chunk = new internal::Uninitialized<T>[chunk_size];
      for (std::size_t i = 0; i < chunk_size; i++) {
        constructor(static_cast<void*>(chunk[i].get()));
      }
      chunks[chunk_id].store(chunk, std::memory_order_release);
    }
  }

  std::function<void(void*)> constructor;
  std::mutex growing_mutex;
  std::array<std::atomic<internal::Uninitialized<T>*>, 25> chunks;

  // 25 chunks guarantees enough slots for any machine
  // with up to 2^48 bytes of addressable virtual memory,
  // assuming that threads are 8MB large.
};

}  // namespace parlay

#endif  // PARLAY_THREAD_LOCAL_H_
