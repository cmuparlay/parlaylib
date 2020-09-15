#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline unsigned int num_workers() { return 1;}
inline unsigned int worker_id() { return 0;}
inline void set_num_workers(int) { ; }

template <class F>
inline void parallel_for(long start, long end, F f,
			 long,
			 bool) {
  for (long i=start; i<end; i++) {
    f(i);
  }
}

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool) {
  left(); right();
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_

