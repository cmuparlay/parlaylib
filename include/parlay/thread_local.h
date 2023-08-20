
#ifndef PARLAY_THREAD_LOCAL_H_
#define PARLAY_THREAD_LOCAL_H_

#include <cstddef>

#include <array>
#include <atomic>
#include <mutex>
#include <thread>

#include "internal/thread_id_pool.h"

#include "portability.h"

namespace parlay {

// Returns a unique thread ID for the current running thread
// in the range of 0...num_thread_ids().  Thread IDs are
// guaranteed to be unique for all *live* threads, but they
// are re-used after a thread dies and another is spawned.
inline std::size_t my_thread_id() {
  return internal::get_thread_id();
}

// Return the number of thread IDs that have been assigned to
// threads.  All thread IDs are in the range 0...num_thread_ids()
inline std::size_t num_thread_ids() {
  return internal::get_num_thread_ids();
}

// Ensure that the thread ID pool is initialized by touching the pool.
// Otherwise, this function doesn't actually do anything.
inline void initialize_thread_ids() {
  my_thread_id();
}

// A PerThreadList<T> stores a list of objects of type T such that there
// is a unique object for each active thread. The list automatically grows
// when additional threads are spawned and attempt to access it. Threads
// may also traverse the entire list if they need to.
//
// A few things to note:
// - List elements are all value initialized, roughly meaning that class types
//   are default constructed, and builtin types are zero initialized.
//
// - Thread IDs are always unique for the set of currently live threads,
//   but not unique over the course of the entire program.  A thread that
//   dies will give up its ID to be claimed by a new thread later.
//
// - The list elements are *not* destroyed when the thread that "owns"
//   them is destroyed. A new thread that reclaims a previously-used ID
//   will find the item at that position in the same state that it was
//   left by the previous thread.  Elements are only destroyed when the
//   entire PerThreadList is destroyed.
template<typename T>
class PerThreadList {

 public:

  PerThreadList() {
    chunks[0].store(new T[internal::ThreadIdPool::thread_list_chunk_size] {}, std::memory_order_relaxed);
  }

  T& get() {
    auto thread_data = internal::get_thread_info();
    return get_by_index(thread_data.chunk_id, thread_data.chunk_position);
  }

  template<typename F>
  void for_each(F&& f) {
    static_assert(std::is_invocable_v<F, T&>);

    auto num_threads = num_thread_ids();
    std::size_t tid = 0;
    T* chunk = chunks[0].load(std::memory_order_relaxed);

    for (std::size_t j = 0; chunk != nullptr; chunk = chunks[++j].load(std::memory_order_acquire)) {
      auto chunk_size = get_chunk_size(j);
      for (std::size_t i = 0; tid < num_threads && i < chunk_size; i++, tid++) {
        f(chunk[i]);
      }
    }
  }

 private:

  std::size_t get_chunk_size(std::size_t chunk_id) {
    if (chunk_id == 0) return internal::ThreadIdPool::thread_list_chunk_size;
    else return internal::ThreadIdPool::thread_list_chunk_size << (chunk_id - 1);
  }

  T& get_by_index(std::size_t chunk_id, std::size_t chunk_position) {
    if (chunk_id > 0 && chunks[chunk_id].load(std::memory_order_acquire) == nullptr) PARLAY_UNLIKELY
      ensure_chunk_exists(chunk_id);
    assert(chunks[chunk_id].load() != nullptr);
    return chunks[chunk_id].load(std::memory_order_relaxed)[chunk_position];
  }

  void ensure_chunk_exists(std::size_t chunk_id) {
    std::lock_guard<std::mutex> lock(growing_mutex);
    if (chunks[chunk_id].load(std::memory_order_relaxed) == nullptr) {
      chunks[chunk_id].store(new T[get_chunk_size(chunk_id)] {}, std::memory_order_release);
    }
  }

  std::mutex growing_mutex;
  std::array<std::atomic<T*>, 25> chunks;   // 25 chunks guarantees enough slots for any machine
                                            // with up to 2^48 bytes of addressable virtual memory
};


}  // namespace parlay


#endif  // PARLAY_THREAD_LOCAL_H_