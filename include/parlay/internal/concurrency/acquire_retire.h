
#ifndef PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
#define PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H

#include <cstddef>

#include <atomic>
#include <memory>
#include <type_traits>
#include <vector>

#include "../../parallel.h"
#include "../../utilities.h"

namespace parlay {
namespace internal {
namespace concurrency {

//
template<typename T, typename Deleter = std::default_delete<T>, size_t delay = 1>
class acquire_retire {

 public:

  acquire_retire() :
    num_threads(num_workers()),
    announcements(num_threads),
    in_progress(num_threads),
    retired(num_threads),
    work(num_threads) {}

  template<typename U>
  U acquire(const std::atomic<U>* p) {
    static_assert(std::is_convertible_to_v<U, T*>, "acquire must read from a type that is convertible to T*");
    size_t id = worker_id();
    U result;
    do {
      result = p->load(std::memory_order_seq_cst);
      PARLAY_PREFETCH(result, 0, 0);
      announcements[id].store(static_cast<T*>(result), std::memory_order_seq_cst);
    } while (p->load(std::memory_order_seq_cst) != result);
    return result;
  }

  void release() {
    size_t id = worker_id();
    announcements[id].store(nullptr);
  }

  template<typename U>
  void retire(U p) {
    static_assert(std::is_convertible_to_v<U, T*>, "retire must take a type that is convertible to T*");
    size_t id = worker_id();
    retired[id].push_back(static_cast<T*>(p));

  }

 private:
  size_t num_threads;
  std::vector<padded<std::atomic<T*>> announcements;
  std::vector<padded<bool>> in_progress;
  std::vector<padded<std::vector<T*>> retired;
  std::vector<padded<size_t>> work;
};

}
}
}

#endif  // PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
