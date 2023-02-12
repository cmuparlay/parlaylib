
#ifndef PARLAY_INTERNAL_WORK_STEALING_JOB_H_
#define PARLAY_INTERNAL_WORK_STEALING_JOB_H_

#include <cassert>

#include <atomic>
#include <type_traits>

namespace parlay {

// Jobs are thunks -- i.e., functions that take no arguments
// and return nothing. Could be a lambda, e.g. [] () {}.

struct WorkStealingJob {
  WorkStealingJob() {
    done.store(false, std::memory_order_relaxed);
  }
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

// Holds a type-specific reference to a callable object
template<typename F>
struct JobImpl : WorkStealingJob {
  static_assert(std::is_invocable_v<F&>);
  explicit JobImpl(F& _f) : WorkStealingJob(), f(_f) { }
  void execute() override {
    f();
  }
 private:
  F& f;
};

template<typename F>
JobImpl<F> make_job(F& f) { return JobImpl(f); }

}

#endif  // PARLAY_INTERNAL_WORK_STEALING_JOB_H_
