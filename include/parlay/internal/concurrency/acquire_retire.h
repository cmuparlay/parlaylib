
#ifndef PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
#define PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H

#include <cstddef>

#include <atomic>
#include <memory>
#include <type_traits>
#include <unordered_set>
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
    amortized_work(num_threads) {}

  template<typename U>
  U acquire(const std::atomic<U>* p) {
    static_assert(std::is_convertible_v<U, T*>, "acquire must read from a type that is convertible to T*");
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
    static_assert(std::is_convertible_v<U, T*>, "retire must take a type that is convertible to T*");
    size_t id = worker_id();
    retired[id].push_back(static_cast<T*>(p));
    work_toward_ejects();
  }

  // Perform any remaining deferred destruction. Need to be very careful
  // about additional objects being queued for deferred destruction by
  // an object that was just destructed.
  ~acquire_retire() {
    in_progress.assign(in_progress.size(), true);
    Deleter deleter;

    // Loop because the destruction of one object could trigger the deferred
    // destruction of another object (possibly even in another thread), and
    // so on recursively.
    while (std::any_of(retired.begin(), retired.end(),
                       [](const auto &v) { return !v.empty(); })) {

      // Move all of the contents from the deferred destruction lists
      // into a single local list. We don't want to just iterate the
      // deferred lists because a destruction may trigger another
      // deferred destruction to be added to one of the lists, which
      // would invalidate its iterators
      std::vector<T*> destructs;
      for (auto &v : retired) {
        destructs.insert(destructs.end(), v.begin(), v.end());
        v.clear();
      }

      // Perform all of the pending deferred destructions
      for (auto x : destructs) {
        deleter(x);
      }
    }
  }

 private:
  // Apply the function f to every currently announced handle
  template<typename F>
  void scan_slots(F &&f) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    for (const auto &announcement_slot : announcements) {
      auto x = announcement_slot.load(std::memory_order_seq_cst);
      if (x != nullptr) f(x);
    }
  }

  void work_toward_ejects(size_t work = 1) {
    auto id = worker_id();
    amortized_work[id] = amortized_work[id] + work;
    auto threshold = std::max<size_t>(30, delay * amortized_work.size());  // Always attempt at least 30 ejects
    while (!in_progress[id] && amortized_work[id] >= threshold) {
      amortized_work[id] = 0;
      if (retired[id].size() == 0) break; // nothing to collect
      in_progress[id] = true;
      Deleter deleter;
      auto deferred = padded<std::vector<T*>>(std::move(retired[id]));

      std::unordered_multiset<T*> announced;
      scan_slots([&](auto reserved) { announced.insert(reserved); });

      // For a given deferred decrement, we first check if it is announced, and, if so,
      // we defer it again. If it is not announced, it can be safely applied. If an
      // object is deferred / announced multiple times, each announcement only protects
      // against one of the deferred decrements, so for each object, the amount of
      // decrements applied in total will be #deferred - #announced
      auto f = [&](auto x) {
        auto it = announced.find(x);
        if (it == announced.end()) {
          deleter(x);
          return true;
        } else {
          announced.erase(it);
          return false;
        }
      };

      // Remove the deferred decrements that are successfully applied
      deferred.erase(remove_if(deferred.begin(), deferred.end(), f), deferred.end());
      retired[id].insert(retired[id].end(), deferred.begin(), deferred.end());
      in_progress[id] = false;
    }
  }

  size_t num_threads;
  std::vector<padded<std::atomic<T*>>> announcements;
  std::vector<padded<bool>> in_progress;
  std::vector<padded<std::vector<T*>>> retired;
  std::vector<padded<size_t>> amortized_work;
};

}  // namespace concurrency
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
