// EXAMPLE USE 1:
//
// fork_join_scheduler fj;
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
#include <vector>

#include "internal/work_stealing_job.h"

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
    tag_t tag;
    qidx top;
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
 public:
  // see comments under wait(..)
  static bool const conservative = false;
  unsigned int num_threads;

  static thread_local unsigned int thread_id;

  scheduler()
      : num_threads(init_num_workers()),
        num_deques(2 * num_threads),
        deques(num_deques),
        attempts(num_deques),
        spawned_threads(),
        finished_flag(false) {
    // Stopping condition
    auto finished = [this]() {
      return finished_flag.load(std::memory_order_relaxed);
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
    finished_flag.store(true, std::memory_order_relaxed);
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
  void wait(F finished, bool conservative = false) {
    // Conservative avoids deadlock if scheduler is used in conjunction
    // with user locks enclosing a wait.
    if (conservative) {
      while (!finished()) std::this_thread::yield();
    }
    // If not conservative, schedule within the wait.
    // Can deadlock if a stolen job uses same lock as encloses the wait.
    else
      start(finished);
  }

  // All scheduler threads quit after this is called.
  void finish() { finished_flag.store(true, std::memory_order_relaxed); }

  // Pop from local stack.
  Job* try_pop() {
    auto id = worker_id();
    return deques[id].pop_bottom();
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)  // 'getenv': This function or variable may be unsafe.
#endif

  // Determine the number of workers to spawn
  unsigned int init_num_workers() {
    if (const auto env_p = std::getenv("PARLAY_NUM_THREADS")) {
      return std::stoi(env_p);
    } else {
      return std::thread::hardware_concurrency();
    }
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  unsigned int num_workers() { return num_threads; }
  unsigned int worker_id() { return thread_id; }
  void set_num_workers(unsigned int) {
    std::cout << "Unsupported" << std::endl;
    exit(-1);
  }

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
  void start(F finished) {
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
  Job* get_job(F finished) {
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
  fork_join_scheduler() : sched(std::make_unique<scheduler<Job>>()) {}

  unsigned int num_workers() { return sched->num_workers(); }
  unsigned int worker_id() { return sched->worker_id(); }
  void set_num_workers(int n) { sched->set_num_workers(n); }

  // Fork two thunks and wait until they both finish.
  template <typename L, typename R>
  void pardo(L left, R right, bool conservative = false) {
    auto right_job = make_job(right);
    sched->spawn(&right_job);
    left();
    if (sched->try_pop() != nullptr)
      right();
    else {
      auto finished = [&]() { return right_job.finished(); };
      sched->wait(finished, conservative);
    }
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

  template <typename F>
  size_t get_granularity(size_t start, size_t end, F f) {
    size_t done = 0;
    size_t sz = 1;
    int ticks = 0;
    do {
      sz = std::min(sz, end - (start + done));
      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i = 0; i < sz; i++) f(start + done + i);
      auto tstop = std::chrono::high_resolution_clock::now();
      ticks = static_cast<int>((tstop - tstart).count());
      done += sz;
      sz *= 2;
    } while (ticks < 1000 && done < (end - start));
    return done;
  }

  template <typename F>
  void parfor(size_t start, size_t end, F f, size_t granularity = 0,
              bool conservative = false) {
    if (end <= start) return;
    if (granularity == 0) {
      size_t done = get_granularity(start, end, f);
      granularity = std::max(done, (end - start) / (128 * sched->num_threads));
      parfor_(start + done, end, f, granularity, conservative);
    } else
      parfor_(start, end, f, granularity, conservative);
  }

 private:
  template <typename F>
  void parfor_(size_t start, size_t end, F f, size_t granularity,
               bool conservative) {
    if ((end - start) <= granularity)
      for (size_t i = start; i < end; i++) f(i);
    else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative
      // caches on powers of 2.
      size_t mid = (start + (9 * (n + 1)) / 16);
      pardo([&]() { parfor_(start, mid, f, granularity, conservative); },
            [&]() { parfor_(mid, end, f, granularity, conservative); },
            conservative);
    }
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

};

}  // namespace parlay

#endif  // PARLAY_SCHEDULER_H_
