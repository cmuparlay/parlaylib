
#ifndef PARLAY_KTH_SMALLEST_H_
#define PARLAY_KTH_SMALLEST_H_

#include "sequence_ops.h"
#include "random.h"

namespace parlay {

  template <class Seq, class Compare>
  typename Seq::value_type kth_smallest(Seq const &s, size_t k, Compare less,
					random r = random()) {
    using T = typename Seq::value_type;
    size_t n = s.size();
    T pivot = s[r[0]%n];
    sequence<T> smaller = filter(s, [&] (T a) {return less(a, pivot);});
    if (k < smaller.size())
      return kth_smallest(smaller, k, less, r.next());
    else {
      sequence<T> larger = filter(s, [&] (T a) {return less(pivot, a);});
      if (k >= n - larger.size())
	return kth_smallest(larger, k - n + larger.size(), less, r.next());
      else return pivot;
    }
  }

  template <class Seq, class Compare>
  typename Seq::value_type approximate_kth_smallest(Seq const &S, size_t k, Compare less, random r = random()) {
    // raise exception if empty sequence?
    using T = typename Seq::value_type;
    size_t n = S.size();
    size_t num_samples = n/sqrt(n);
    parlay::sequence<T> samples(num_samples, [&] (size_t i) -> T {
	return S[r[i]%n];});
    return sample_sort(samples, less)[k * num_samples / n];
  }
  
}  // namespace parlay

#endif  // PARLAY_KTH_SMALLEST_H_
