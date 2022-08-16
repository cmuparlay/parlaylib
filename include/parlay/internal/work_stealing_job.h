
#ifndef PARLAY_INTERNAL_WORK_STEALING_JOB_H_
#define PARLAY_INTERNAL_WORK_STEALING_JOB_H_

#include <cassert>

#include <atomic>
#include <utility>

namespace parlay {

// Jobs are thunks -- i.e., functions that take no arguments
// and return nothing. Could be a lambda, e.g. [] () {}.

struct WorkStealingJob {
  WorkStealingJob() : done(false) { }
  ~WorkStealingJob() = default;
  void operator()() {
    assert(done.load(std::memory_order_relaxed) == false);
    execute();
    done.store(true, std::memory_order_release);
  }
  [[nodiscard]] bool finished() const noexcept {
    return done.load(std::memory_order_acquire);
  }
 protected:
  virtual void execute() = 0;
  std::atomic<bool> done;
};

// Holds a callable object
template<typename F>
struct JobImpl : WorkStealingJob {
  explicit JobImpl(F&& _f, int) : WorkStealingJob(), f(static_cast<F&&>(_f)) { }
  void execute() override { static_cast<F&&>(f)(); }
  F f;
};

template<typename F>
auto make_job(F&& f) { return JobImpl<F>(std::forward<F>(f), 0); }

}

#endif  // PARLAY_INTERNAL_WORK_STEALING_JOB_H_
