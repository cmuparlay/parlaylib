#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_

#include <omp.h>

#include <atomic>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline size_t num_workers() { return omp_get_max_threads(); }
inline size_t worker_id() { return omp_get_thread_num(); }

namespace internal {
extern inline std::atomic_flag& in_omp_parallel() {
  static std::atomic_flag f;
  return f;
}
}

template <class F>
inline void parallel_for(size_t start, size_t end, F f, long, bool) {
  bool was_in_par = internal::in_omp_parallel().test_and_set();
  _Pragma("omp parallel for")
    for(size_t i=start; i<end; i++) f(i);
  if (!was_in_par) internal::in_omp_parallel().clear();
}

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool) {
  if (internal::in_omp_parallel().test_and_set() == false) {    // at top level start up tasking
#pragma omp parallel
    {
#pragma omp single
      {
#pragma omp task
        {
          left();
        }
#pragma omp task
        {
          right();
        }
      }
    }
#pragma omp taskwait
    internal::in_omp_parallel().clear();
  }
  else {   // already started
#pragma omp task
    {
      left();
    }
#pragma omp task
    {
      right();
    }
#pragma omp taskwait
  }
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_
