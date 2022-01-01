#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_
#if defined(PARLAY_SEQUENTIAL)

#include <stddef.h>

namespace parlay {

// IWYU pragma: private, include "../../parallel.h"

inline size_t num_workers() { return 1;}
inline size_t worker_id() { return 0;}
inline void set_num_workers(int) { ; }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

template <class F>
inline void parallel_for(size_t start, size_t end, F f,
                         long, bool) {
  for (size_t i=start; i<end; i++) {
    f(i);
  }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename Lf, typename Rf>
inline void par_do(Lf left, Rf right, bool) {
  left(); right();
}

}  // namespace parlay

#endif
#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_SEQUENTIAL_HPP_

