
#ifndef PARLAY_PARALLEL_H_
#define PARLAY_PARALLEL_H_

//****************************************************
// All algorithms in the library use only four functions for
// accessing parallelism. These can be implemented on
// top of any scheduler.
//****************************************************

namespace parlay {

// number of threads available from OS
static int num_workers();

// id of running thread, should be numbered from [0...num-workers)
static int worker_id();

// parallel loop from start (inclusive) to end (exclusive) running
// function f.
//    f should map long to void.
//    granularity is the number of iterations to run sequentially
//      if 0 (default) then the scheduler will decide
//    conservative uses a safer scheduler
template <typename F>
static void parallel_for(long start, long end, F f,
			 long granularity = 0,
			 bool conservative = false);

// runs the thunks left and right in parallel.
//    both left and write should map void to void
//    conservative uses a safer scheduler
template <typename Lf, typename Rf>
static void par_do(Lf left, Rf right, bool conservative=false);

}  // namespace parlay

//****************************************************
//                Scheduler selection
//****************************************************

#if defined(PARLAY_CILK)
#include "internal/scheduler_plugins/cilk.h"            // IWYU pragma: keep
#elif defined(PARLAY_OPENMP)
#include "internal/scheduler_plugins/omp.h"             // IWYU pragma: keep
#elif defined(PARLAY_TBB)
#include "internal/scheduler_plugins/tbb.h"             // IWYU pragma: keep
#elif defined(PARLAY_SEQUENTIAL)
#include "internal/scheduler_plugins/sequential.h"      // IWYU pragma: keep
#else
#include "internal/scheduler_plugins/parlay.h"          // IWYU pragma: keep
#endif

#endif  // PARLAY_PARALELL_H_

