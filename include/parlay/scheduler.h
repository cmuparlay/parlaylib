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

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>         // IWYU pragma: keep
#include <memory>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <utility>
#include <vector>

#include "internal/atomic_wait.h"
#include "internal/work_stealing_deque.h"
#include "internal/work_stealing_job.h"

#include <string>
#include <sstream>

// IWYU pragma: no_include <bits/chrono.h>
// IWYU pragma: no_include <bits/this_thread_sleep.h>

namespace parlay {

template <typename Job>
struct scheduler {
  static_assert(std::is_invocable_r_v<void, Job&>);

  // After YIELD_FACTOR * P unsuccessful steal attempts, a
  // a worker will yield to give other threads a chance
  // to work and save some cycles.
  // 
  // After SLEEP_FACTOR rounds of unsuccessful steals, a worker
  // will go to sleep until it is notified that there is more
  // work to steal, in order to save CPU time
  constexpr static size_t YIELD_FACTOR = 200;
  constexpr static size_t SLEEP_FACTOR = 1000;

  // After 1000ms (1 second) of unsuccessful steals, a worker will go
  // to sleep until it is notified that there is more work to steal i
  // to save CPU time
  constexpr static std::chrono::milliseconds SLEEP_DELAY{1000};

 public:
  unsigned int num_threads;

  static thread_local unsigned int thread_id;
  
  std::atomic<size_t> wake_up_counter{0};
  std::atomic<size_t> num_finished_workers{0};

  explicit scheduler(size_t num_workers)
      : num_threads(num_workers),
        num_deques(num_threads),
        deques(num_deques),
        attempts(num_deques),
        spawned_threads{} {
    
    // Spawn num_threads many threads on startup
    thread_id = 0;  // thread-local write
    for (unsigned int i = 1; i < num_threads; i++) {
      spawned_threads.emplace_back([&, i]() {
        worker(i);
      });
    }
  }

  ~scheduler() {
    shutdown();
  }

  // Spawn a new job that can run in parallel.
  void spawn(Job* job) {
    int id = worker_id();
    bool first = deques[id].push_bottom(job);
    if (first) wake_up_a_worker();
  }

  // Wait until the given job is complete.
  //
  // If conservative, this thread will simply wait until
  // the job is complete. Otherwise, it will look for work
  // to steal and keep busy until the job is complete.
  void wait_for(Job& job, bool conservative) {
    if (conservative) {
      job.wait();
    }
    else {
      do_work_until_complete(job);
    }
  }

  // Pop from local stack.
  Job* get_own_job() {
    auto id = worker_id();
    return deques[id].pop_bottom();
  }

  unsigned int num_workers() const noexcept { return num_threads; }
  unsigned int worker_id() const noexcept { return thread_id; }

  bool finished() const noexcept {
    return finished_flag.load(std::memory_order_acquire);
  }

 private:
  // Align to avoid false sharing.
  struct alignas(128) attempt {
    size_t val;
  };

  int num_deques;
  std::vector<internal::Deque<Job>> deques;
  std::vector<attempt> attempts;
  std::vector<std::thread> spawned_threads;
  std::atomic<bool> finished_flag{false};

  // Start an individual scheduler task that looks for
  // jobs to steal. Runs until the scheduler's finished
  // flag is set to true.
  void worker(size_t id) {
    thread_id = id;  // thread-local write
    //wait_for_work();
    while (!finished()) {
      Job* job = get_job([&]() { return finished(); });
      if (job) {
        (*job)();
      }
      else if (!finished()) {
        // If no job was stolen, the worker should go to
        // sleep and wait until more work is available
        wait_for_work();
      }
    }
    num_finished_workers.fetch_add(1);
  }

  // Keeps busy by looking for work (either in the threads own
  // local stack or by stealing if the local stack is empty) until
  // the given (stolen) job has been completed (by someone else).
  void do_work_until_complete(Job& waiting_job) {
    while (!waiting_job.finished()) {
      Job* job = get_job([&]() { return waiting_job.finished(); });
      if (job) {
        (*job)();
      }
    }
  }

  // Find a job, first trying local stack, then random steals.
  template <typename F>
  Job* get_job(F&& break_early) {
    if (break_early()) return nullptr;
    Job* job = get_own_job();
    if (job) return job;
    else job = steal_job(std::move(break_early));
    return job;
  }
  
  template<typename F>
  Job* steal_job(F&& break_early) {
    size_t id = worker_id();
    const auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < SLEEP_DELAY) {
      // By coupon collector's problem, this should touch all.
      for (size_t i = 0; i <= YIELD_FACTOR * num_deques; i++) {
        if (break_early()) return nullptr;
        Job* job = try_steal(id);
        if (job) return job;
      }
      std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));
    }
    return nullptr;
  }
  
  Job* try_steal(size_t id) {
    // use hashing to get "random" target
    size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
    attempts[id].val++;
    auto [job, empty] = deques[target].pop_top();
    if (!empty) wake_up_a_worker();
    return job;
  }

  // Wakes up at least one sleeping worker (more than one
  // worker may be woken up depending on the implementation).
  void wake_up_a_worker() {
    wake_up_counter.fetch_add(1);
    parlay::atomic_notify_one(&wake_up_counter);
  }
  
  // Wake up all sleeping workers
  void wake_up_all_workers() {
    wake_up_counter.fetch_add(1);
    parlay::atomic_notify_all(&wake_up_counter);
  }
  
  // Wait until notified to wake up
  void wait_for_work() {
    parlay::atomic_wait(&wake_up_counter, wake_up_counter.load());
  }

  size_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }
  
  void shutdown() {
    finished_flag.store(true, std::memory_order_release);
    // We must spam wake all workers until they finish in
    // case any of them are just about to fall asleep, since
    // they might therefore miss the flag to finish
    while (num_finished_workers.load() < num_threads - 1) {
      wake_up_all_workers();
      std::this_thread::yield();
    }
    for (unsigned int i = 1; i < num_threads; i++) {
      spawned_threads[i - 1].join();
    }
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
    auto execute_right = [&]() { std::forward<R>(right)(); };
    auto right_job = make_job(right);
    sched->spawn(&right_job);
    std::forward<L>(left)();
    if (const Job* job = sched->get_own_job(); job != nullptr) {
      assert(job == &right_job);
      execute_right();
    }
    else if (!right_job.finished()) {
      sched->wait_for(right_job, conservative);
    }
  }

  template <typename F>
  void parfor(size_t start, size_t end, F f, size_t granularity = 0, bool conservative = false) {
    if (end <= start) return;
    if (granularity == 0) {
      size_t done = get_granularity(start, end, f);
      granularity = std::max(done, (end - start) / static_cast<size_t>(128 * sched->num_threads));
      start += done;
    }
    parfor_(start, end, f, granularity, conservative);
  }

 private:
  template <typename F>
  size_t get_granularity(size_t start, size_t end, F f) {
    size_t done = 0;
    size_t sz = 1;
    unsigned long long int ticks = 0;
    do {
      sz = std::min(sz, end - (start + done));
      auto tstart = std::chrono::steady_clock::now();
      for (size_t i = 0; i < sz; i++) f(start + done + i);
      auto tstop = std::chrono::steady_clock::now();
      ticks = static_cast<unsigned long long int>(std::chrono::duration_cast<
                std::chrono::nanoseconds>(tstop - tstart).count());
      done += sz;
      sz *= 2;
    } while (ticks < 1000 && done < (end - start));
    return done;
  }

  template <typename F>
  void parfor_(size_t start, size_t end, F f, size_t granularity, bool conservative) {
    if ((end - start) <= granularity)
      for (size_t i = start; i < end; i++) f(i);
    else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative caches on powers of 2.
      size_t mid = (start + (9 * (n + 1)) / 16);
      pardo([&]() { parfor_(start, mid, f, granularity, conservative); },
            [&]() { parfor_(mid, end, f, granularity, conservative); },
            conservative);
    }
  }

};

}  // namespace parlay

#endif  // PARLAY_SCHEDULER_H_
