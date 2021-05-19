
#ifndef PARLAY_PARALLEL_H_
#define PARLAY_PARALLEL_H_

#include <cstddef>

//****************************************************
// All algorithms in the library use only four functions for
// accessing parallelism. These can be implemented on
// top of any scheduler.
//****************************************************

namespace parlay {

// number of threads available from OS
inline size_t num_workers();

// id of running thread, should be numbered from [0...num-workers)
inline size_t worker_id();

// parallel loop from start (inclusive) to end (exclusive) running
// function f.
//    f should map long to void.
//    granularity is the number of iterations to run sequentially
//      if 0 (default) then the scheduler will decide
//    conservative uses a safer scheduler
template <typename F>
inline void parallel_for(size_t start, size_t end, F f, long granularity = 0,
                         bool conservative = false);

// runs the thunks left and right in parallel.
//    both left and write should map void to void
//    conservative uses a safer scheduler
template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool conservative = false);

}  // namespace parlay

//****************************************************
//                Scheduler selection
//****************************************************

// Cilk Scheduler
#if defined(PARLAY_CILK)
#include "internal/scheduler_plugins/cilk.h"        // IWYU pragma: keep, export

// OpenMP Scheduler
#elif defined(PARLAY_OPENMP)
#include "internal/scheduler_plugins/omp.h"         // IWYU pragma: keep, export

// Intel's Thread Building Blocks Scheduler
#elif defined(PARLAY_TBB)
#include "internal/scheduler_plugins/tbb.h"         // IWYU pragma: keep, export

// No Parallelism
#elif defined(PARLAY_SEQUENTIAL)
#include "internal/scheduler_plugins/sequential.h"  // IWYU pragma: keep, export

// Parlay's Homegrown Scheduler
#else

#include "scheduler.h"

namespace parlay {
  
namespace internal {

// Use a "Meyer singleton" to provide thread-safe 
// initialisation and destruction of the scheduler
//
// The declaration of get_default_scheduler must be 
// extern inline to ensure that there is only ever one 
// copy of the scheduler. This is guaranteed by the C++
// standard: 7.1.2/4 A static local variable in an
// extern inline function always refers to the same
// object.
extern inline fork_join_scheduler& get_default_scheduler() {
  static fork_join_scheduler fj;
  return fj;
}

}  // namespace internal

inline size_t num_workers() {
  return internal::get_default_scheduler().num_workers();
}

inline size_t worker_id() {
  return internal::get_default_scheduler().worker_id();
}

template <class F>
inline void parallel_for(size_t start, size_t end, F f,
                         long granularity,
			 bool conservative) {
  if (end > start)
    internal::get_default_scheduler().parfor(start, end, f, (size_t) granularity, conservative);
}

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool conservative) {
  return internal::get_default_scheduler().pardo(left, right, conservative);
}

}  // namespace parlay

#endif

#endif  // PARLAY_PARALELL_H_
