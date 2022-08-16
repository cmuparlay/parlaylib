// EXAMPLE USE 1:
//
// fork_join_scheduler fj(num_threads);
//
// long fib(long i) {
//   if (i <= 1) return 1;
//   long l,r;
//   fj.pardo([&] () { l = fib(i-1);},
//            [&] () { r = fib(i-2);});
//   return l + r;
// }
//
// fib(40);
//
// EXAMPLE USE 2:
//
// void init(long* x, size_t n) {
//   parfor(0, n, [&] (int i) {a[i] = i;});
// }
//

#ifndef PARLAY_SCHEDULER_H_
#define PARLAY_SCHEDULER_H_

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <utility>
#include <vector>

#include "internal/work_stealing_job.h"

#include <sstream>

// IWYU pragma: no_include <bits/this_thread_sleep.h>

namespace parlay {

// Deque from Arora, Blumofe, and Plaxton (SPAA, 1998).
template <typename Job>
struct Deque {
  using qidx = unsigned int;
  using tag_t = unsigned int;

  // use std::atomic<age_t> for atomic access.
  // Note: Explicit alignment specifier required
  // to ensure that Clang inlines atomic loads.
  struct alignas(int64_t) age_t {
    // cppcheck bug prevents it from seeing usage with braced initializer
    tag_t tag;                // cppcheck-suppress unusedStructMember
    qidx top;                 // cppcheck-suppress unusedStructMember
  };

  // align to avoid false sharing
  struct alignas(64) padded_job {
    std::atomic<Job*> job;
  };

  static constexpr int q_size = 1000;
  std::atomic<qidx> bot;
  std::atomic<age_t> age;
  std::array<padded_job, q_size> deq;

  Deque() : bot(0), age(age_t{0, 0}) {}

  void push_bottom(Job* job) {
    auto local_bot = bot.load(std::memory_order_relaxed);      // atomic load
    deq[local_bot].job.store(job, std::memory_order_relaxed);  // shared store
    local_bot += 1;
    if (local_bot == q_size) {
      throw std::runtime_error("internal error: scheduler queue overflow");
    }
    bot.store(local_bot, std::memory_order_relaxed);  // shared store
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  Job* pop_top() {
    Job* result = nullptr;
    auto old_age = age.load(std::memory_order_relaxed);    // atomic load
    auto local_bot = bot.load(std::memory_order_relaxed);  // atomic load
    if (local_bot > old_age.top) {
      auto job =
          deq[old_age.top].job.load(std::memory_order_relaxed);  // atomic load
      auto new_age = old_age;
      new_age.top = new_age.top + 1;
      if (age.compare_exchange_strong(old_age, new_age))
        result = job;
      else
        result = nullptr;
    }
    return result;
  }

  Job* pop_bottom() {
    Job* result = nullptr;
    auto local_bot = bot.load(std::memory_order_relaxed);  // atomic load
    if (local_bot != 0) {
      local_bot--;
      bot.store(local_bot, std::memory_order_relaxed);  // shared store
      std::atomic_thread_fence(std::memory_order_seq_cst);
      auto job =
          deq[local_bot].job.load(std::memory_order_relaxed);  // atomic load
      auto old_age = age.load(std::memory_order_relaxed);      // atomic load
      if (local_bot > old_age.top)
        result = job;
      else {
        bot.store(0, std::memory_order_relaxed);  // shared store
        auto new_age = age_t{old_age.tag + 1, 0};
        if ((local_bot == old_age.top) &&
            age.compare_exchange_strong(old_age, new_age))
          result = job;
        else {
          age.store(new_age, std::memory_order_relaxed);  // shared store
          result = nullptr;
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
      }
    }
    return result;
  }
};

template <typename Job>
struct scheduler {
  static_assert(std::is_invocable_r_v<void, Job&>);

 public:
  unsigned int num_threads;

  static thread_local unsigned int thread_id;

  explicit scheduler(size_t num_workers)
      : num_threads(num_workers),
        num_deques(2 * num_threads),
        deques(num_deques),
        attempts(num_deques),
        spawned_threads(),
        finished_flag(false) {
    // Stopping condition
    auto finished = [this]() {
      return finished_flag.load(std::memory_order_acquire);
    };

    // Spawn num_threads many threads on startup
    thread_id = 0;  // thread-local write
    for (unsigned int i = 1; i < num_threads; i++) {
      spawned_threads.emplace_back([&, i, finished]() {
        thread_id = i;  // thread-local write
        start(finished);
      });
    }
  }

  ~scheduler() {
    finished_flag.store(true, std::memory_order_release);
    for (unsigned int i = 1; i < num_threads; i++) {
      spawned_threads[i - 1].join();
    }
  }

  // Push onto local stack.
  void spawn(Job* job) {
    int id = worker_id();
    deques[id].push_bottom(job);
  }

  // Wait for condition: finished().
  template <typename F>
  void wait(F&& finished, bool conservative = false) {
    // Conservative avoids deadlock if scheduler is used in conjunction
    // with user locks enclosing a wait.
    if (conservative) {
      while (!finished())
        std::this_thread::yield();
    }
    // If not conservative, schedule within the wait.
    // Can deadlock if a stolen job uses same lock as encloses the wait.
    else {
      start(std::forward<F>(finished));
    }
  }

  // Pop from local stack.
  Job* try_pop() {
    auto id = worker_id();
    return deques[id].pop_bottom();
  }

  unsigned int num_workers() { return num_threads; }
  unsigned int worker_id() { return thread_id; }

 private:
  // Align to avoid false sharing.
  struct alignas(128) attempt {
    size_t val;
  };

  int num_deques;
  std::vector<Deque<Job>> deques;
  std::vector<attempt> attempts;
  std::vector<std::thread> spawned_threads;
  std::atomic<int> finished_flag;

  // Start an individual scheduler task.  Runs until finished().
  template <typename F>
  void start(F&& finished) {
    while (true) {
      Job* job = get_job(finished);
      if (!job) return;
      (*job)();
    }
  }

  Job* try_steal(size_t id) {
    // use hashing to get "random" target
    size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
    attempts[id].val++;
    return deques[target].pop_top();
  }

  // Find a job, first trying local stack, then random steals.
  template <typename F>
  Job* get_job(F&& finished) {
    if (finished()) return nullptr;
    Job* job = try_pop();
    if (job) return job;
    size_t id = worker_id();
    while (true) {
      // By coupon collector's problem, this should touch all.
      for (int i = 0; i <= num_deques * 100; i++) {
        if (finished()) return nullptr;
        job = try_steal(id);
        if (job) return job;
      }
      // If haven't found anything, take a breather.
      std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));
    }
  }

  size_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }
};

template <typename T>
thread_local unsigned int scheduler<T>::thread_id = 0;

class fork_join_scheduler {
  using Job = WorkStealingJob;

  // Underlying scheduler object
  std::unique_ptr<scheduler<Job>> sched;

 public:
  explicit fork_join_scheduler(size_t p) : sched(std::make_unique<scheduler<Job>>(p)) {}

  unsigned int num_workers() { return sched->num_workers(); }
  unsigned int worker_id() { return sched->worker_id(); }

  // Fork two thunks and wait until they both finish.
  template <typename L, typename R>
  void pardo(L&& left, R&& right, bool conservative = false) {
    auto right_job = make_job(std::forward<R>(right));
    sched->spawn(&right_job);
    std::forward<L>(left)();
    if (sched->try_pop() != nullptr) {
      right_job();
    }
    else {
      wait_for(right_job, conservative);
    }
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

  template <typename F>
  void parfor(size_t start, size_t end, F&& f, size_t granularity = 0, bool conservative = false) {
    assert(start < end);
    if (granularity == 0) {   // use automatic granularity heuristic to pick a granularity
      size_t min_granularity = std::max<size_t>(1, (end - start) / static_cast<size_t>(128 * sched->num_threads));
      size_t done = get_granularity(start, end, f, min_granularity, conservative);
      granularity = std::max<size_t>(done, min_granularity);
      start += done;
    }
    parfor_(start, end, f, granularity, conservative);
  }

 private:
  template <typename F>
  void parfor_(size_t start, size_t end, F&& f, size_t granularity, bool conservative) {
    assert(granularity >= 1);
    assert(start <= end);
    if ((end - start) <= granularity) {
      for (size_t i = start; i < end; i++) f(i);
    }
    else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative caches on powers of 2.
      size_t mid = (start + (9 * (n + 1)) / 16);
      pardo([&]() { parfor_(start, mid, f, granularity, conservative); },
            [&]() { parfor_(mid, end, f, granularity, conservative); },
      conservative);
    }
  }

  template <typename F>
  size_t get_granularity(size_t start, size_t end, F&& f, size_t min_granularity, bool conservative) {
    size_t done = 0;
    size_t sz = 1;
    std::chrono::nanoseconds::rep ticks = 0;

    // In case the loop body is extremely heavy, this allows the remaining tasks
    // to be stolen and parallelized while the first iteration is still running.
    //
    // Without this optimization, a heavy loop like par_for(0, P, { sleep for 1 sec }),
    // will take 2 seconds, because the first iteration is run entirely sequentially
    // before the remaining iterations can be parallelized.
    auto remaining_iterations = make_job([&]() { parfor_(start + 1, end, f, min_granularity, conservative); });
    sched->spawn(&remaining_iterations);

    do {
      sz = std::min(sz, end - (start + done));
      auto tstart = std::chrono::steady_clock::now();
      for (size_t i = 0; i < sz; i++) f(start + done + i);
      auto tstop = std::chrono::steady_clock::now();

      // Early break -- if the remaining iterations got stolen then this
      // must be a heavy loop body, and it has already been parallelized
      if (done == 0 && sched->try_pop() == nullptr) {
        wait_for(remaining_iterations, conservative);
        return end - start;
      }

      ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(tstop - tstart).count();
      done += sz;
      sz *= 2;
    } while (ticks < 1000 && done < (end - start));
    return done;
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  void wait_for(Job& job, bool conservative) {
    sched->wait([&]() { return job.finished(); }, conservative);
  }

};

}  // namespace parlay

#endif  // PARLAY_SCHEDULER_H_
