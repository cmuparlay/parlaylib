#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_CILK_HPP_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_CILK_HPP_

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include <iostream>
#include <sstream>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline unsigned int num_workers() { return __cilkrts_get_nworkers(); }
inline unsigned int worker_id() { return __cilkrts_get_worker_number(); }

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool) {
  cilk_spawn right();
  left();
  cilk_sync;
}

template <typename F>
inline void parallel_for(long start, long end, F f,
			 long granularity,
			 bool) {
  if (granularity == 0)
    cilk_for(long i=start; i<end; i++) f(i);
  else if ((end - start) <= granularity)
    for (long i=start; i < end; i++) f(i);
  else {
    long n = end-start;
    long mid = (start + (9*(n+1))/16);
    cilk_spawn parallel_for(start, mid, f, granularity);
    parallel_for(mid, end, f, granularity);
    cilk_sync;
  }
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_CILK_HPP_

