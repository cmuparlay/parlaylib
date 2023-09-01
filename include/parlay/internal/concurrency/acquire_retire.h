
#ifndef PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
#define PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H

#include <cstddef>

#include <algorithm>      // IWYU pragma: keep
#include <atomic>
#include <memory>
#include <tuple>
#include <type_traits>    // IWYU pragma: keep
#include <unordered_set>
#include <utility>        // IWYU pragma: keep
#include <vector>

#include "../../portability.h"
#include "../../thread_specific.h"
#include "../../utilities.h"

// IWYU pragma: no_forward_declare parlay::padded

namespace parlay {
namespace internal {

// intrusive_acquire_retire works on types T that expose a T* via
// the ADL findable free function intrusive_ar_get_next(ptr)
template<typename T, typename Deleter = std::default_delete<T>, size_t delay = 1>
class intrusive_acquire_retire {

  struct RetiredList {

    constexpr RetiredList() noexcept = default;

    ~RetiredList() {
      assert(size == 0 && "RetiredLists must be emptied via cleanup() before destruction.");
    }

    void push(T* p) noexcept {
      intrusive_ar_get_next(p) = std::exchange(head, p);
      size++;
    }

    [[nodiscard]] std::size_t get_size() const noexcept {
      return size;
    }

    template<typename F>
    void cleanup(F&& is_protected, Deleter& destroy) {

      while (head && !is_protected(head)) {
        T* old = std::exchange(head, head->get_next());
        destroy(old);
        size--;
      }

      if (head) {
        T* prev = head;
        T* current = intrusive_ar_get_next(head);
        while (current) {
          if (!is_protected(current)) {
            intrusive_ar_get_next(prev) = intrusive_ar_get_next(current);
            destroy(current);
            current = intrusive_ar_get_next(prev);
            size--;
          }
          else {
            prev = std::exchange(current, intrusive_ar_get_next(current));
          }
        }
      }
    }

   private:
    T* head = nullptr;
    std::size_t size = 0;
  };

 public:
  explicit intrusive_acquire_retire(Deleter deleter_ = {}) :
    deleter(std::move(deleter_)), data() {}

  template<typename U>
  U acquire(const std::atomic<U>& p) {
    static_assert(std::is_convertible_v<U, T*>, "acquire must read from a type that is convertible to T*");
    U result;
    do {
      result = p.load(std::memory_order_seq_cst);
      PARLAY_PREFETCH(result, 0, 0);
      data->announcement.store(static_cast<T*>(result), std::memory_order_seq_cst);
    } while (p.load(std::memory_order_seq_cst) != result);
    return result;
  }

  void release() {
    data->announcement.store(nullptr);
  }

  template<typename U>
  void retire(U p) {
    static_assert(std::is_convertible_v<U, T*>, "retire must take a type that is convertible to T*");
    data->retired.push(static_cast<T*>(p));
    work_toward_ejects();
  }

  // Perform any remaining deferred destruction. Need to be very careful
  // about additional objects being queued for deferred destruction by
  // an object that was just destructed.
  ~intrusive_acquire_retire() {
    // Not a data race since no thread should be accessing the intrusive_acquire_retire
    // instance while its destructor is being run.  That would be a race from the user.
    data.for_each([](auto& td) {
      td.in_progress = true;
    });

    // Perform cleanup in a loop in case deleting a retired value causes
    // something else to get retired (e.g., cleaning up a linked list)
    bool retired = true;
    while (retired) {
      retired = false;
      data.for_each([&](auto&& local_data) {
        if (local_data.retired.get_size() > 0) {
          retired = true;
          local_data.retired.cleanup([](auto&&) { return false; }, deleter);
        }
      });
    }
  }

 private:
  // Apply the function f to every currently announced value
  template<typename F>
  void scan_slots(F &&f) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    data.for_each([&](auto&& local_data) {
      auto x = local_data.announcement.load(std::memory_order_seq_cst);
      if (x != nullptr) f(x);
    });
  }

  void work_toward_ejects(size_t work = 1) {
    auto id = worker_id();
    data->amortized_work += work;
    auto threshold = std::max<size_t>(30, delay * num_thread_ids());  // Always attempt at least 30 ejects
    while (!in_progress[id] && amortized_work[id] >= threshold) {
      amortized_work[id] = 0;
      if (retired[id].size() == 0) break; // nothing to collect
      in_progress[id] = true;
      auto deferred = padded<std::vector<T*>>(std::move(retired[id]));

      std::unordered_multiset<T*> announced;
      scan_slots([&](auto reserved) { announced.insert(reserved); });

      // For a given deferred destruct, we first check if it is announced, and, if so,
      // we defer it again. If it is not announced, it can be safely applied. If an
      // object is deferred / announced multiple times, each announcement only protects
      // against one of the deferred destructs, so for each object, the amount of
      // destructs applied in total will be #deferred - #announced
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

      // Remove the deferred destructs that are successfully applied
      deferred.erase(remove_if(deferred.begin(), deferred.end(), f), deferred.end());
      retired[id].insert(retired[id].end(), deferred.begin(), deferred.end());
      in_progress[id] = false;
    }
  }

  Deleter& get_deleter() noexcept {
    return deleter;
  }

  const Deleter& get_deleter() const noexcept {
    return deleter;
  }

  struct ThreadData {
    std::atomic<T*> announcement{nullptr};
    bool in_progress{false};
    size_t amortized_work{0};
    RetiredList retired{};
  };

  ThreadSpecific<ThreadData> data;

  //std::unique_ptr<padded<std::atomic<T*>>[]> announcements;
  //std::unique_ptr<padded<bool>[]> in_progress;
  //std::unique_ptr<padded<std::vector<T*>>[]> retired;
  //std::unique_ptr<padded<size_t>[]> amortized_work;
  PARLAY_NO_UNIQUE_ADDR Deleter deleter;
};


}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_CONCURRENCY_ACQUIRE_RETIRE_H
