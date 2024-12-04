// For automatically generating a parlay::sequence in rapidcheck

#ifndef PARLAY_RAPID_CHECK_UTILS
#define PARLAY_RAPID_CHECK_UTILS

#include "rapidcheck.h"

#include "parlay/sequence.h"

namespace rc {

template<typename T>
struct Arbitrary<parlay::sequence<T>> {
  static Gen<parlay::sequence<T>> arbitrary() {
    auto r = gen::map<std::vector<T>>([](std::vector<T> x){
      return parlay::sequence<T>(x.begin(), x.end());
    });
    return r;
  }
};

} // namespace rc

#endif  // PARLAY_RAPID_CHECK_UTILS
