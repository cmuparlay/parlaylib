
#ifndef PARLAY_INTERNAL_THREAD_ID_POOL_H_
#define PARLAY_INTERNAL_THREAD_ID_POOL_H_

#include <cassert>
#include <cstddef>

#include <atomic>
#include <thread>
#include <utility>

namespace parlay {
namespace internal {

// A ThreadIdPool hands out and maintains available unique dense IDs for active threads.
// Each thread that requests an ID will get one in the range from 0...get_num_thread_ids().
// When the pool runs out of available IDs, it will allocate new ones, increasing the result
// of get_num_thread_ids().  Threads that die will return their ID to the pool for re-use by
// a subsequently spawned thread.
//
// There is a global singleton instance of ThreadIdPool given by ThreadIdPool::instance(),
// however this function is private and should not be called by the outside world. The public
// API through which the world can access thread IDs is limited to the free functions:
//
// - get_thread_id() -> size_t:  Returns the thread ID of the current thread. Will assign
//                               one if this thread doesn't have one yet.
// - get_num_thread_ids() -> size_t:  Returns the number of unique thread IDs that have
//                                    been handed out.
//
class ThreadIdPool {

 public:

  // This is just std::bit_ceil(std::thread::hardware_concurrency()) but we don't assume C++20
  const static inline std::size_t thread_list_chunk_size = []() {
    std::size_t size = 4;
    while (size < std::thread::hardware_concurrency())
      size *= 2;
    return size;
  }();

  // Used by PerThreadList which stores a chunked sequence of items that is at least as large
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

 private:

  ThreadIdPool() noexcept : num_thread_ids(0), available_ids(nullptr) { }

  ThreadIdPool(const ThreadIdPool&) = delete;
  ThreadIdPool& operator=(const ThreadIdPool&) = delete;

  ~ThreadIdPool() noexcept {
    size_t num_destroyed = 0;
    ThreadId* current = available_ids.load(std::memory_order_relaxed);
    while (current) {
      auto old = std::exchange(current, current->next);
      delete old;
      num_destroyed++;
    }
    if (num_destroyed != num_thread_ids.load(std::memory_order_relaxed)) {
      std::cerr << "There are thread IDs which have been handed out but not returned to the global pool"
                << " and the global pool is now being destroyed.  To fix this, you need to ensure that"
                   " the global pool is initialized before any thread that invokes any Parlay code is"
                   " spawned. You can do this by calling parlay::initialize_thread_ids() before spawning"
                   " any threads that will make use of Parlay.";
    }
    assert(num_destroyed == num_thread_ids.load(std::memory_order_relaxed));
  }

  class ThreadId {
    friend class ThreadIdPool;

    ThreadId(const std::size_t id_) : id(id_), next(nullptr) { }

   public:
    const std::size_t id;
   private:
    ThreadId* next;
  };

  struct ThreadIdOwner {
    friend class ThreadIdPool;

    ThreadIdOwner() : node(instance().acquire()), id(node->id),
        chunk_id(compute_chunk_id(id)), chunk_position(compute_chunk_position(id, chunk_id)) {
    }

    ~ThreadIdOwner() { instance().relinquish(node); }

   private:

    ThreadId* const node;

   public:
    const std::size_t id;
    const std::size_t chunk_id;
    const std::size_t chunk_position;
  };

  ThreadId* acquire() {
    ThreadId* current = available_ids.load();
    while (current && !available_ids.compare_exchange_weak(current, current->next)) {}
    if (current) { return current; }
    else { return new ThreadId(num_thread_ids.fetch_add(1)); }
  }

  void relinquish(ThreadId* p) {
    p->next = available_ids.load();
    while (!available_ids.compare_exchange_weak(p->next, p)) {}
  }

  static inline ThreadIdOwner& get_local_thread_id() {
    static thread_local ThreadIdPool::ThreadIdOwner my_id;
    return my_id;
  }

  static inline ThreadIdPool& instance() {
    static ThreadIdPool pool;
    return pool;
  }

 public:

  friend std::size_t get_thread_id();

  friend std::size_t get_num_thread_ids();

  friend ThreadIdOwner get_thread_info();

 private:
  std::atomic<size_t> num_thread_ids;
  std::atomic<ThreadId*> available_ids;
};

inline std::size_t get_thread_id() {
  return ThreadIdPool::get_local_thread_id().id;
}

inline std::size_t get_num_thread_ids() {
  return ThreadIdPool::instance().num_thread_ids.load();
}

inline ThreadIdPool::ThreadIdOwner get_thread_info() {
  return ThreadIdPool::get_local_thread_id();
}

}  // namespace internal
}  // namespace parlay


#endif  // PARLAY_INTERNAL_THREAD_ID_POOL_H_
