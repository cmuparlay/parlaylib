
#ifndef PARLAY_SCHEDULER_H_
#define PARLAY_SCHEDULER_H_

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <chrono>         // IWYU pragma: keep
#include <memory>
#include <thread>
#include <type_traits>    // IWYU pragma: keep
#include <utility>
#include <vector>

#include "internal/work_stealing_deque.h"         // IWYU pragma: keep
#include "internal/work_stealing_job.h"

// IWYU pragma: no_include <bits/chrono.h>
// IWYU pragma: no_include <bits/this_thread_sleep.h>



// True if the scheduler should scale the number of awake workers
// proportional to the amount of work to be done. This saves CPU
// time if there is not any parallel work available, but may cause
// some startup lag when more parallelism becomes available.
//
// Default: true
#ifndef PARLAY_ELASTIC_PARALLELISM
#define PARLAY_ELASTIC_PARALLELISM true
#endif


// PARLAY_ELASTIC_STEAL_TIMEOUT sets the number of microseconds
// that a worker will attempt to steal jobs, such that if no
// jobs are successfully stolen, it will go to sleep.
//
// Default: 10000 (10 milliseconds)
#ifndef PARLAY_ELASTIC_STEAL_TIMEOUT
#define PARLAY_ELASTIC_STEAL_TIMEOUT 10000
#endif


#if PARLAY_ELASTIC_PARALLELISM
#include "internal/atomic_wait.h"
#endif

namespace parlay {


template <typename Job>
struct scheduler {

  using worker_id_type = unsigned int;

 private:
  static_assert(std::is_invocable_r_v<void, Job&>);

  struct workerInfo {
    static constexpr worker_id_type UNINITIALIZED = std::numeric_limits<worker_id_type>::max();

    worker_id_type worker_id;
    scheduler* my_scheduler;

    workerInfo() : worker_id(UNINITIALIZED), my_scheduler(nullptr) {}
    workerInfo(std::size_t worker_id_, scheduler* s) : worker_id(worker_id_), my_scheduler(s) {}

    workerInfo& operator=(const workerInfo&) = delete;
    workerInfo(const workerInfo&) = delete;

    workerInfo& operator=(workerInfo&& w) noexcept {
      if (this != &w) {
        worker_id = std::exchange(w.worker_id, UNINITIALIZED);
        my_scheduler = std::exchange(w.my_scheduler, nullptr);
      }
      return *this;
    }

    workerInfo(workerInfo&& w) noexcept { *this = std::move(w); }
  };

  // After YIELD_FACTOR * P unsuccessful steal attempts, a
  // a worker will sleep briefly for SLEEP_FACTOR * P nanoseconds
  // to give other threads a chance to work and save some cycles.
  constexpr static size_t YIELD_FACTOR = 200;
  constexpr static size_t SLEEP_FACTOR = 200;

  // The length of time that a worker must fail to steal anything
  // before it goes to sleep to save CPU time.
  constexpr static std::chrono::microseconds STEAL_TIMEOUT{PARLAY_ELASTIC_STEAL_TIMEOUT};

  static inline thread_local workerInfo worker_info{};

 public:

  const worker_id_type num_threads;

  // If the current thread is a worker of an existing scheduler, or the thread that spawned
  // a scheduler, return the most recent such scheduler.  Otherwise, returns null.
  static scheduler* get_current_scheduler() {
    return worker_info.my_scheduler;
  }

  explicit scheduler(size_t num_workers)
      : num_threads(num_workers),
        num_deques(num_threads),
        num_awake_workers(num_threads),
        parent_worker_info(std::exchange(worker_info, workerInfo{0, this})),
        deques(num_deques),
        attempts(num_deques),
        spawned_threads(),
        finished_flag(false) {

    // Spawn num_threads many threads on startup
    for (worker_id_type i = 1; i < num_threads; ++i) {
      spawned_threads.emplace_back([&, i]() {
        worker_info = {i, this};
        worker();
      });
    }
  }

  ~scheduler() {
    shutdown();
    worker_info = std::move(parent_worker_info);
  }

  // Push onto local stack.
  void spawn(Job* job) {
    int id = worker_id();
    [[maybe_unused]] bool first = deques[id].push_bottom(job);
#if PARLAY_ELASTIC_PARALLELISM
    if (first) wake_up_a_worker();
#endif
  }

  // Wait until the given condition is true.
  //
  // If conservative, this thread will simply busy wait. Otherwise,
  // it will look for work to steal and keep itself occupied. This
  // can deadlock if the stolen work wants a lock held by the code
  // that is waiting, so avoid that.
  template <typename F>
  void wait_until(F&& done, bool conservative = false) {
    // Conservative avoids deadlock if scheduler is used in conjunction
    // with user locks enclosing a wait.
    if (conservative) {
      while (!done())
        std::this_thread::yield();
    }
    // If not conservative, schedule within the wait.
    // Can deadlock if a stolen job uses same lock as encloses the wait.
    else {
      do_work_until(std::forward<F>(done));
    }
  }

  // Pop from local stack.
  Job* get_own_job() {
    auto id = worker_id();
    return deques[id].pop_bottom();
  }

  worker_id_type num_workers() { return num_threads; }
  worker_id_type worker_id() { return worker_info.worker_id; }

  bool finished() const noexcept {
    return finished_flag.load(std::memory_order_acquire);
  }

 private:
  // Align to avoid false sharing.
  struct alignas(128) attempt {
    size_t val;
  };

  int num_deques;
  std::atomic<size_t> num_awake_workers;
  workerInfo parent_worker_info;
  std::vector<internal::Deque<Job>> deques;
  std::vector<attempt> attempts;
  std::vector<std::thread> spawned_threads;
  std::atomic<int> finished_flag;

  std::atomic<size_t> wake_up_counter{0};
  std::atomic<size_t> num_finished_workers{0};

  // Start an individual worker task, stealing work if no local
  // work is available. May go to sleep if no work is available
  // for a long time, until woken up again when notified that
  // new work is available.
  void worker() {
#if PARLAY_ELASTIC_PARALLELISM
    wait_for_work();
#endif
    while (!finished()) {
      Job* job = get_job([&]() { return finished(); }, PARLAY_ELASTIC_PARALLELISM);
      if (job)(*job)();
#if PARLAY_ELASTIC_PARALLELISM
      else if (!finished()) {
        // If no job was stolen, the worker should go to
        // sleep and wait until more work is available
        wait_for_work();
      }
#endif
    }
    assert(finished());
    num_finished_workers.fetch_add(1);
  }

  // Runs tasks until done(), stealing work if necessary.
  //
  // Does not sleep or time out since this can be called
  // by the main thread and by join points, for which sleeping
  // would cause deadlock, and timing out could cause a join
  // point to resume execution before the job it was waiting
  // on has completed.
  template <typename F>
  void do_work_until(F&& done) {
    while (true) {
      Job* job = get_job(done, false);  // timeout MUST BE false
      if (!job) return;
      (*job)();
    }
    assert(done());
  }

  // Find a job, first trying local stack, then random steals.
  //
  // Returns nullptr if break_early() returns true before a job
  // is found, or, if timeout is true and it takes longer than
  // STEAL_TIMEOUT to find a job to steal.
  template <typename F>
  Job* get_job(F&& break_early, bool timeout) {
    if (break_early()) return nullptr;
    Job* job = get_own_job();
    if (job) return job;
    else job = steal_job(std::forward<F>(break_early), timeout);
    return job;
  }
  
  // Find a job with random steals.
  //
  // Returns nullptr if break_early() returns true before a job
  // is found, or, if timeout is true and it takes longer than
  // STEAL_TIMEOUT to find a job to steal.
  template<typename F>
  Job* steal_job(F&& break_early, bool timeout) {
    size_t id = worker_id();
    const auto start_time = std::chrono::steady_clock::now();
    do {
      // By coupon collector's problem, this should touch all.
      for (size_t i = 0; i <= YIELD_FACTOR * num_deques; i++) {
        if (break_early()) return nullptr;
        Job* job = try_steal(id);
        if (job) return job;
      }
      std::this_thread::sleep_for(std::chrono::nanoseconds(num_deques * 100));
    } while (!timeout || std::chrono::steady_clock::now() - start_time < STEAL_TIMEOUT);
    return nullptr;
  }

  Job* try_steal(size_t id) {
    // use hashing to get "random" target
    size_t target = (hash(id) + hash(attempts[id].val)) % num_deques;
    attempts[id].val++;
    auto [job, empty] = deques[target].pop_top();
#if PARLAY_ELASTIC_PARALLELISM
    if (!empty) wake_up_a_worker();
#endif
    return job;
  }

#if PARLAY_ELASTIC_PARALLELISM

  // Wakes up at least one sleeping worker (more than one
  // worker may be woken up depending on the implementation).
  void wake_up_a_worker() {
    if (num_awake_workers.load(std::memory_order_acquire) < num_threads) {
      wake_up_counter.fetch_add(1);
      parlay::atomic_notify_one(&wake_up_counter);
    }
  }
  
  // Wake up all sleeping workers
  void wake_up_all_workers() {
    if (num_awake_workers.load(std::memory_order_acquire) < num_threads) {
      wake_up_counter.fetch_add(1);
      parlay::atomic_notify_all(&wake_up_counter);
    }
  }
  
  // Wait until notified to wake up
  void wait_for_work() {
    num_awake_workers.fetch_sub(1);
    parlay::atomic_wait(&wake_up_counter, wake_up_counter.load());
    num_awake_workers.fetch_add(1);
  }

#endif

  size_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }
  
  void shutdown() {
    finished_flag.store(true, std::memory_order_release);
#if PARLAY_ELASTIC_PARALLELISM
    // We must spam wake all workers until they finish in
    // case any of them are just about to fall asleep, since
    // they might therefore miss the flag to finish
    while (num_finished_workers.load() < num_threads - 1) {
      wake_up_all_workers();
      std::this_thread::yield();
    }
#endif
    for (worker_id_type i = 1; i < num_threads; ++i) {
      spawned_threads[i - 1].join();
    }
  }
};


class fork_join_scheduler {
  using Job = WorkStealingJob;
  using scheduler_t = scheduler<Job>;

 public:

  // Fork two thunks and wait until they both finish.
  template <typename L, typename R>
  static void pardo(scheduler_t& scheduler, L&& left, R&& right, bool conservative = false) {
    auto execute_right = [&]() { std::forward<R>(right)(); };
    auto right_job = make_job(right);
    scheduler.spawn(&right_job);
    std::forward<L>(left)();
    if (const Job* job = scheduler.get_own_job(); job != nullptr) {
      assert(job == &right_job);
      execute_right();
    }
    else {
      auto done = [&]() { return right_job.finished(); };
      scheduler.wait_until(done, conservative);
      assert(right_job.finished());
    }
  }

  template <typename F>
  static void parfor(scheduler_t& scheduler, size_t start, size_t end, F&& f, size_t granularity = 0, bool conservative = false) {
    if (end <= start) return;
    if (granularity == 0) {
      size_t done = get_granularity(start, end, f);
      granularity = std::max(done, (end - start) / static_cast<size_t>(128 * scheduler.num_threads));
      start += done;
    }
    parfor_(scheduler, start, end, f, granularity, conservative);
  }

 private:
  template <typename F>
  static size_t get_granularity(size_t start, size_t end, F& f) {
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
  static void parfor_(scheduler_t& scheduler, size_t start, size_t end, F& f, size_t granularity, bool conservative) {
    if ((end - start) <= granularity)
      for (size_t i = start; i < end; i++) f(i);
    else {
      size_t n = end - start;
      // Not in middle to avoid clashes on set-associative caches on powers of 2.
      size_t mid = (start + (9 * (n + 1)) / 16);
      pardo(scheduler,
            [&]() { parfor_(scheduler, start, mid, f, granularity, conservative); },
            [&]() { parfor_(scheduler, mid, end, f, granularity, conservative); },
            conservative);
    }
  }

};

}  // namespace parlay

#endif  // PARLAY_SCHEDULER_H_
