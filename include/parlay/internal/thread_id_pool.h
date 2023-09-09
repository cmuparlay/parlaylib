
#ifndef PARLAY_INTERNAL_THREAD_ID_POOL_H_
#define PARLAY_INTERNAL_THREAD_ID_POOL_H_

#include <cassert>
#include <cstddef>

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

namespace parlay {
namespace internal {

using thread_id_type = unsigned int;

// A ThreadIdPool hands out and maintains available unique dense IDs for active threads.
// Each thread that requests an ID will get one in the range from [0...get_num_thread_ids()).
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
class ThreadIdPool : public std::enable_shared_from_this<ThreadIdPool> {

  // Prevent public construction since this class is meant as a global singleton
  struct private_constructor {
    explicit private_constructor() = default;
  };

 public:

  // Returns a unique thread ID for the current thread in the range [0...get_num_thread_ids())
  friend thread_id_type get_thread_id();

  // Returns the number of assigned thread IDs in the range [0...get_num_thread_ids())
  friend thread_id_type get_num_thread_ids();


  ~ThreadIdPool() noexcept {
    size_t num_destroyed = 0;
    for (auto current = available_ids.load(std::memory_order_relaxed); current; num_destroyed++) {
      auto old = std::exchange(current, current->next);
      delete old;
    }
    assert(num_destroyed == num_thread_ids.load(std::memory_order_relaxed));
  }

  // The constructor must be public since we make_shared it, but we protect it with a private parameter type
  explicit ThreadIdPool(private_constructor) noexcept : num_thread_ids(0), available_ids(nullptr) { }

  ThreadIdPool(const ThreadIdPool&) = delete;
  ThreadIdPool& operator=(const ThreadIdPool&) = delete;

 private:

  // A ThreadId corresponds to a unique ID number in the range [0...num_thread_ids). When it is
  // not in use (the thread that owned it dies), it is returned to the global pool which maintains
  // a linked list of available ones.
  class ThreadId {
    friend class ThreadIdPool;

    explicit ThreadId(const thread_id_type id_) noexcept : id(id_), next(nullptr) { }

   public:
    const thread_id_type id;
   private:
    ThreadId* next;
  };

  // A ThreadIdOwner indicates that a thread is currently in possession of the given ThreadID.
  // Each thread has a static thread_local ThreadIdOwner containing the ID that it owns.
  // On construction, it acquires an available ThreadID, and on destruction, it releases
  // it back to the pool. The ThreadIdOwner stores a shared_ptr to the pool to guarantee
  // that the pool does not get destroyed before a detached thread returns its ID.
  class ThreadIdOwner {
    friend class ThreadIdPool;

    explicit ThreadIdOwner(ThreadIdPool& pool_)
        : pool(pool_.shared_from_this()), node(pool->acquire()), id(node->id) { }

    ~ThreadIdOwner() { pool->relinquish(node); }

   private:
    const std::shared_ptr<ThreadIdPool> pool;
    ThreadId* const node;

   public:
    const thread_id_type id;
  };

  // Grab a free ID from the available list, or if there are none available, allocate a new one.
  ThreadId* acquire() {
    if (available_ids.load(std::memory_order_relaxed)) {
      // We only take the lock if there are available IDs in the pool. In the common case
      // where there are no relinquished IDs available for re-use we don't need the lock.
      static std::mutex m_;
      std::lock_guard<std::mutex> g_{m_};

      ThreadId* current = available_ids.load(std::memory_order_relaxed);
      while (current && !available_ids.compare_exchange_weak(current, current->next,
               std::memory_order_acquire, std::memory_order_relaxed)) {}
      if (current) { return current; }
    }
    return new ThreadId(num_thread_ids.fetch_add(1));
  }

  // Given the ID back to the global pool for reuse
  void relinquish(ThreadId* p) {
    p->next = available_ids.load(std::memory_order_relaxed);
    while (!available_ids.compare_exchange_weak(p->next, p,
             std::memory_order_release, std::memory_order_relaxed)) {}
  }

  static inline const ThreadIdOwner& get_local_thread_id() {
    static const thread_local ThreadIdPool::ThreadIdOwner my_id(instance());
    return my_id;
  }

  static inline ThreadIdPool& instance() {
    // We hold the global thread id pool inside a shared_ptr because it is possible
    // for threads to be spawned *before* the ID pool has been initialized, which
    // means that they may outlive this static variable. Each ThreadId holds onto
    // a copy of the shared_ptr to ensure that the pool stays alive long enough
    // for the IDs to relinquish themselves back to the pool.
    //
    // I think it is still possible to cause a segfault by spawning a new thread
    // *after* the static destructors have run... so please do not spawn threads
    // inside your static destructors :)
    static const std::shared_ptr<ThreadIdPool> pool = std::make_shared<ThreadIdPool>(private_constructor{});
    return *pool;
  }

  std::atomic<size_t> num_thread_ids;
  std::atomic<ThreadId*> available_ids;
};

inline thread_id_type get_thread_id() {
  return ThreadIdPool::get_local_thread_id().id;
}

inline thread_id_type get_num_thread_ids() {
  return ThreadIdPool::instance().num_thread_ids.load();
}


}  // namespace internal
}  // namespace parlay


#endif  // PARLAY_INTERNAL_THREAD_ID_POOL_H_
