// Scheduler plugin for OpenMP
//
// Critical Note: It is **very important** that we do not accidentally nest
// parallel regions in OpenMP, because this will result in duplicate worker
// IDs (each team gets assigned their own sequential worker IDs from 0 to
// omp_get_num_threads() - 1). Therefore, we always check whether we are
// already inside a parallel region before creating one. If we are already
// inside one, tasks will just make use of the existing threads in the team.
//

#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_

#include <omp.h>

#include <type_traits>
#include <utility>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline size_t num_workers() {
  return omp_get_max_threads();
}

inline size_t worker_id() {
  return omp_get_thread_num();
}


template <typename F>
inline void parallel_for(size_t start, size_t end, F&& f, long granularity, bool) {
  static_assert(std::is_invocable_v<F&, size_t>);

  if (end == start + 1) {
    f(start);
  }
  else if ((end - start) <= static_cast<size_t>(granularity)) {
    for (size_t i=start; i < end; i++) {
      f(i);
    }
  }
  else if (!omp_in_parallel()) {
    #pragma omp parallel
    {
      #pragma omp single
      {
        if (granularity <= 1) {
          #pragma omp taskloop
          for (size_t i = start; i < end; i++) {
            f(i);
          }
        }
        else {
          #pragma omp taskloop grainsize(granularity)
          for (size_t i = start; i < end; i++) {
            f(i);
          }
        }
      }
    }
  }
  else {
    if (granularity <= 1) {
      #pragma omp taskloop shared(f)
      for (size_t i = start; i < end; i++) {
        f(i);
      }
    }
    else {
      #pragma omp taskloop grainsize(granularity) shared(f)
      for (size_t i = start; i < end; i++) {
        f(i);
      }
    }
  }
}

template <typename Lf, typename Rf>
inline void par_do(Lf&& left, Rf&& right, bool) {
  static_assert(std::is_invocable_v<Lf&&>);
  static_assert(std::is_invocable_v<Rf&&>);

  // If we are not yet in a parallel region, start one
  if (!omp_in_parallel()) {
    #pragma omp parallel
    {
      #pragma omp single
      {
        #pragma omp taskgroup
        {
          #pragma omp task untied
          { std::forward<Rf>(right)(); }

          { std::forward<Lf>(left)(); }
        }
      }  // omp single
    }  // omp parallel
  }
  // Already inside a parallel region, avoid creating nested one (see comment at top)
  else {
    #pragma omp taskgroup
    {
      #pragma omp task untied shared(right)
      { std::forward<Rf>(right)(); }

      { std::forward<Lf>(left)(); }
    }
  }
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_OMP_H_
