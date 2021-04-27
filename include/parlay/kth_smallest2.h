
#ifndef PARLAY_KTH_SMALLEST_H_
#define PARLAY_KTH_SMALLEST_H_

#include "random.h"
#include "sequence_ops.h"

namespace parlay {

template <class Seq, class Compare>
typename Seq::value_type kth_smallest_old(Seq const &s, size_t k, Compare less,
                                      random r = random()) {
  using T = typename Seq::value_type;
  size_t n = s.size();
  T pivot = s[r[0] % n];
  sequence<T> smaller = filter(s, [&](T a) { return less(a, pivot); });
  if (k < smaller.size())
    return kth_smallest_old(smaller, k, less, r.next());
  else {
    sequence<T> larger = filter(s, [&](T a) { return less(pivot, a); });
    if (k >= n - larger.size())
      return kth_smallest_old(larger, k - n + larger.size(), less, r.next());
    else
      return pivot;
  }
}

template <class Seq, class Compare>
typename Seq::value_type approximate_kth_smallest(Seq const &S, size_t k,
                                                  Compare less,
                                                  random r = random()) {
  // raise exception if empty sequence?
  using T = typename Seq::value_type;
  size_t n = S.size();
  size_t num_samples = n / sqrt(n);
  parlay::sequence<T> samples(num_samples,
                              [&](size_t i) -> T { return S[r[i] % n]; });
  return sample_sort(samples, less)[k * num_samples / n];
}

template <class Iterator, class Compare>
auto kth_smallest(slice<Iterator, Iterator> S,
                  size_t k,
				          Compare less,
				          random r = random()) {

  // raise exception if empty sequence?
  using T = typename slice<Iterator, Iterator>::value_type;
  size_t n = S.size();
  if (n < 100) return sort(S, less)[k];    

  // picks a sample of size sqrt(n), sorts it, and uses it to guess region
  // in which k falls
  size_t num_samples = n / sqrt(n);
  size_t sqrt_samples = sqrt(num_samples);
  auto samples = parlay::sequence<T>::from_function(num_samples, [&] (auto i) {
      return S[r[i] % n]; });
  auto s = sample_sort(make_slice(samples), less);
  auto a = s[std::max((k * num_samples / n), (size_t) sqrt_samples)
	     - sqrt_samples];
  auto b = s[std::min((k * num_samples / n) + sqrt_samples, num_samples-1)];
  auto l = count_if(S, [&] (T x) {return less(x, a);});
  auto h = count_if(S, [&] (T x) {return less(x, b);});

  // k should fall between l and h, but if not we have to recurse on
  // one of the two sides
  if (k == l) return a;
  else if (k < l) {
    auto left = filter(s, [&] (T x) { return less(x, a);});
    return kth_smallest(make_slice(left), k, less, r.next());
  }
  else if (k == h) {
    return b;
  }
  else if (k > h) {
    auto m = count_if(S, [&] (T x) {return !less(a, x);});
    if (k < m) return a;
    auto left = filter(s, [&] (T x) { return less(a, x);});
    return kth_smallest(make_slice(left), k-m, less, r.next());
  } else { // k is between l and h
    if (!less(a,b)) return a;
    // the common case
    auto x = filter(s, [&](T c) { return !less(c, a) && less(c,b);});
    assert(x.size() >= k);
    auto sorted = sample_sort(make_slice(x), less);
    assert(sorted.size() == x.size());
    assert(sorted.size() >= k);
    return sorted[k - l];
  }
}

}  // namespace parlay

#endif  // PARLAY_KTH_SMALLEST_H_
