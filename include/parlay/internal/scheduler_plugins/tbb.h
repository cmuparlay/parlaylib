#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_TBB_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_TBB_H_

#include <cstddef>

#include <type_traits>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_invoke.h>
#include <tbb/task_arena.h>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline size_t num_workers() { return tbb::this_task_arena::max_concurrency(); }

inline size_t worker_id() {
  auto id = tbb::this_task_arena::current_thread_index();
  return id == tbb::task_arena::not_initialized ? 0 : id;
}

template <typename F>
inline void parallel_for(size_t start, size_t end, F&& f, long granularity, bool) {
  static_assert(std::is_invocable_v<F&, size_t>);
  // Use TBB's automatic granularity partitioner (tbb::auto_partitioner)
  if (granularity == 0) {
    tbb::parallel_for(tbb::blocked_range<size_t>(start, end), [&](const tbb::blocked_range<size_t>& r) {
      for (auto i = r.begin(); i != r.end(); ++i) {
        f(i);
      }
    }, tbb::auto_partitioner{});
  }
  // Otherwise, use the granularity specified by the user (tbb::simple_partitioner)
  else {
    tbb::parallel_for(tbb::blocked_range<size_t>(start, end, granularity), [&](const tbb::blocked_range<size_t>& r) {
      for (auto i = r.begin(); i != r.end(); ++i) {
        f(i);
      }
    }, tbb::simple_partitioner{});
  }
}

template <typename Lf, typename Rf>
inline void par_do(Lf&& left, Rf&& right, bool) {
  static_assert(std::is_invocable_v<Lf&&>);
  static_assert(std::is_invocable_v<Rf&&>);
  tbb::parallel_invoke(std::forward<Lf>(left), std::forward<Rf>(right));
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_TBB_H_

