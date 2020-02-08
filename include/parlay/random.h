
#ifndef PARLAY_RANDOM_H_
#define PARLAY_RANDOM_H_

#include "utilities.h"

namespace parlay {

// A cheap version of an inteface that should be improved
// Allows forking a state into multiple states
struct random {
 public:
  random(size_t seed) : state(seed){};
  random() : state(0){};
  random fork(uint64_t i) const { return random(hash64(hash64(i + state))); }
  random next() const { return fork(0); }
  size_t ith_rand(uint64_t i) const { return hash64(i + state); }
  size_t operator[](size_t i) const { return ith_rand(i); }
  size_t rand() { return ith_rand(0); }

 private:
  uint64_t state = 0;
};

}  // namespace parlay

#endif  // PARLAY_RANDOM_H_
