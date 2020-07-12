
#ifndef PARLAY_PRIMITIVES_H_
#define PARLAY_PRIMITIVES_H_

#include "internal/collect_reduce.h"
#include "internal/sequence_ops.h"
#include "internal/sample_sort.h"

#include "monoid.h"
#include "range.h"
#include "slice.h"

namespace parlay {
  
// Internal dispatch functions
  
namespace internal {
  
// Sort an integer_sequence
template<PARLAY_RANGE_TYPE R>
auto sort_dispatch(const R& in, /* is_integral */ std::true_type) {
  return integer_sort(in);
}

// Sort a non-integer sequence
template<PARLAY_RANGE_TYPE R>
auto sort_dispatch(const R& in, /* is_integral */ std::false_type) {
  using value_type = range_value_type_t<R>;
  return sample_sort(make_slice(in), std::less<value_type>{});
}

// Sort an integer sequence in place
template<PARLAY_RANGE_TYPE R>
auto sort_inplace_dispatch(R& in, /* is_integral */ std::true_type) {
  return integer_sort_inplace(in);
}

// Sort a non-integer sequence in place
template<PARLAY_RANGE_TYPE R>
auto sort_inplace_dispatch(R& in, /* is_integral */ std::false_type) {
  using value_type = range_value_type_t<R>;
  return sample_sort_inplace(make_slice(in), std::less<value_type>{});
}

}  // namespace internal

/* -------------------- Map and Tabulate -------------------- */

// Return a sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename UnaryOp>
auto tabulate(size_t n, UnaryOp&& f) {
  return sequence<typename std::remove_reference<
                  typename std::remove_cv<
                  decltype(f(0))
                  >::type>::type>::
                  from_function(n, f);
}

// Return a sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto map(R&& r, UnaryOp&& f) {
  return tabulate(parlay::size(r), [&](size_t i) { return f(std::begin(r)[i]); });
}

// Return a delayed sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
//
// If r is a temporary, the delayed sequence will take
// ownership of it by moving it. If r is a reference,
// the delayed sequence will hold a reference to it, so
// r must remain alive as long as the delayed sequence.
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto dmap(R&& r, UnaryOp&& f) {
  size_t n = parlay::size(r);
  return delayed_sequence<typename std::remove_reference<
                          typename std::remove_cv<
                          decltype(f(std::declval<range_value_type_t<R>&>()))
                          >::type>::type>
    (n, [ r = std::forward<R>(r), f = std::forward<UnaryOp>(f) ]
      (size_t i) { return f(std::begin(r)[i]); });
}

/* -------------------- Copying -------------------- */

// Copy the elements from the range in into the range out
// The range out must be at least as large as in.
template<PARLAY_RANGE_TYPE R_in, PARLAY_RANGE_TYPE R_out>
void copy(const R_in& in, R_out& out) {
  assert(parlay::size(out) >= parlay::size(in));
  parallel_for(0, parlay::size(in), [&](size_t i) {
    std::begin(out)[i] = std::begin(in)[i];
  });
}

/* ---------------------- Reduce ---------------------- */

// Compute the reduction of the elements of r with respect
// to m. That is, given a sequence r[0], ..., r[n-1], and
// an associative operation +, compute
//  r[0] + r[1] + ... + r[n-1]
template<PARLAY_RANGE_TYPE R, typename Monoid>
auto reduce(const R& r, Monoid&& m) {
  return internal::reduce(make_slice(r), std::forward<Monoid>(m));
}

// Compute the sum of the elements of r
template<PARLAY_RANGE_TYPE R>
auto reduce(const R& r) {
  using value_type = range_value_type_t<R>;
  return reduce(r, addm<value_type>());
}

/* ---------------------- Scans --------------------- */

template<PARLAY_RANGE_TYPE R>
auto scan(const R& r) {
  using value_type = range_value_type_t<R>;
  return internal::scan(make_slice(r), addm<value_type>());
}

template<PARLAY_RANGE_TYPE R>
auto scan_inclusive(const R& r) {
  using value_type = range_value_type_t<R>;
  return internal::scan(make_slice(r), addm<value_type>(),
    internal::fl_scan_inclusive);
}

template<PARLAY_RANGE_TYPE R>
auto scan_inplace(R&& r) {
  using value_type = range_value_type_t<R>;
  return internal::scan_inplace(make_slice(r), addm<value_type>());
}

template<PARLAY_RANGE_TYPE R>
auto scan_inclusive_inplace(R&& r) {
  using value_type = range_value_type_t<R>;
  return internal::scan_inplace(make_slice(r), addm<value_type>(),
    internal::fl_scan_inclusive);
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan(const R& r, Monoid&& m) {
  return internal::scan(make_slice(r), std::forward<Monoid>(m));
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan_inclusive(const R& r, Monoid&& m) {
  return internal::scan(make_slice(r), std::forward<Monoid>(m),
    internal::fl_scan_inclusive);
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan_inplace(R&& r, Monoid&& m) {
  return internal::scan_inplace(make_slice(r), std::forward<Monoid>(m));
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan_inclusive_inplace(R& r, Monoid&& m) {
  return internal::scan_inplace(make_slice(r), std::forward<Monoid>(m),
    internal::fl_scan_inclusive);
}

/* ----------------------- Pack ----------------------- */

template<PARLAY_RANGE_TYPE R, PARLAY_RANGE_TYPE BoolSeq>
auto pack(const R& r, const BoolSeq& b) {
  static_assert(std::is_convertible<decltype(*std::begin(b)), bool>::value);
  return internal::pack(make_slice(r), make_slice(b));
}

template<PARLAY_RANGE_TYPE R_in, PARLAY_RANGE_TYPE BoolSeq, PARLAY_RANGE_TYPE R_out>
auto pack_into(const R_in& in, const BoolSeq& b, R_out& out) {
  static_assert(std::is_convertible<decltype(*std::begin(b)), bool>::value);
  return internal::pack_out(make_slice(in), b, make_slice(out));
}

// Return a sequence consisting of the indices such that
// b[i] is true, or is implicitly convertible to true.
template<PARLAY_RANGE_TYPE BoolSeq>
auto pack_index(const BoolSeq& b) {
  return internal::pack_index<size_t>(make_slice(b));
}

// Return a sequence consisting of the indices such that
// b[i] is true, or is implicitly convertible to true.
template<typename IndexType, PARLAY_RANGE_TYPE BoolSeq>
auto pack_index(const BoolSeq& b) {
  return internal::pack_index<IndexType>(make_slice(b));
}

/* ----------------------- Filter --------------------- */

// Return a sequence consisting of the elements x of
// r such that f(x) == true.
template<PARLAY_RANGE_TYPE R, typename UnaryPred>
auto filter(const R& r, UnaryPred&& f) {
  return internal::filter(make_slice(r), std::forward<UnaryPred>(f));
}

template<PARLAY_RANGE_TYPE R_in, PARLAY_RANGE_TYPE R_out, typename UnaryPred>
auto filter_into(const R_in& in, R_out& out, UnaryPred&& f) {
  return internal::filter_out(make_slice(in), make_slice(out), std::forward<UnaryPred>(f));
}

/* ----------------------- Partition --------------------- */

// TODO: Partition

/* ----------------------- Histograms --------------------- */

// Compute a histogram of the values of A, with m buckets.
template<PARLAY_RANGE_TYPE R, typename Integer_>
auto histogram(const R& A, Integer_ m) {
  // We currently have to make a copy of the sequence since
  // histogram internally calls integer sort, which has a
  // const correctness bug and tries to modify the input.
  // TODO: Fix this.
  auto s = parlay::to_sequence(A);
  return internal::histogram(make_slice(s), m);
}

/* -------------------- General Sorting -------------------- */

namespace internal {
  
// We are happy to copy objects that are trivially copyable
// and at most 16 bytes large. This is used to choose between
// using sample sort, which makes copies, or quicksort and
// merge sort, which do not.
template<typename T>
struct okay_to_copy : public std::integral_constant<bool,
    std::is_trivially_copy_constructible_v<T> &&
    std::is_trivially_copy_assignable_v<T>    &&
    std::is_trivially_destructible_v<T>       &&
    sizeof(T) <= 16
> {};

}

// Sort the given sequence and return the sorted sequence
template<PARLAY_RANGE_TYPE R>
auto sort(const R& in) {
  using value_type = range_value_type_t<R>;
  return internal::sample_sort(make_slice(in), std::less<value_type>());
}

// Sort the given sequence with respect to the given
// comparison operation. Elements are sorted such that
// for an element x < y in the sequence, comp(x,y) = true
template<PARLAY_RANGE_TYPE R, typename Compare>
auto sort(const R& in, Compare&& comp) {
  return internal::sample_sort(make_slice(in), std::forward<Compare>(comp));
}

template<PARLAY_RANGE_TYPE R>
auto stable_sort(const R& in) {
  using value_type = range_value_type_t<R>;
  return internal::sample_sort(make_slice(in), std::less<value_type>{}, true);
}

template<PARLAY_RANGE_TYPE R, typename Compare>
auto stable_sort(const R& in, Compare&& comp) {
  return internal::sample_sort(make_slice(in), std::forward<Compare>(comp), true);
}

template<PARLAY_RANGE_TYPE R, typename Compare>
void sort_inplace(R&& in, Compare&& comp) {
  using value_type = range_value_type_t<R>;
  // Could use tag dispatch instead of constexpr 
  // if to make it compatible with C++14
  if constexpr (internal::okay_to_copy<value_type>::value) {
    internal::sample_sort_inplace(make_slice(in), std::forward<Compare>(comp), false);
  }
  else {
    internal::quicksort(make_slice(in), std::forward<Compare>(comp));
  }
}

template<PARLAY_RANGE_TYPE R>
void sort_inplace(R&& in) {
  using value_type = range_value_type_t<R>;
  sort_inplace(std::forward<R>(in), std::less<value_type>{});
}

template<PARLAY_RANGE_TYPE R, typename Compare>
void stable_sort_inplace(R&& in, Compare&& comp) {
  using value_type = range_value_type_t<R>;
  // Could use tag dispatch instead of constexpr 
  // if to make it compatible with C++14
  if constexpr (internal::okay_to_copy<value_type>::value) {
    internal::sample_sort_inplace(make_slice(in), std::forward<Compare>(comp), true);
  }
  else {
    internal::merge_sort_inplace(make_slice(in), std::forward<Compare>(comp));
  }
}

template<PARLAY_RANGE_TYPE R>
void stable_sort_inplace(R&& in) {
  using value_type = range_value_type_t<R>;
  stable_sort_inplace(std::forward<R>(in), std::less<value_type>{});
}



/* -------------------- Integer Sorting -------------------- */

// Note: There is currently no stable integer sort.

// For now, integer_sort makes a copy and then calls
// integer_sort_inplace. Calling internal::integer_sort
// directly currently has const correctness bugs.
// TODO: Fix these bugs

template<PARLAY_RANGE_TYPE R>
auto integer_sort(const R& in) {
  auto s = parlay::to_sequence(in);
  internal::integer_sort_inplace(make_slice(s), [](auto x) { return x; });
  return s;
}

template<PARLAY_RANGE_TYPE R, typename Key>
auto integer_sort(const R& in, Key&& key) {
  auto s = parlay::to_sequence(in);
  internal::integer_sort_inplace(make_slice(s), std::forward<Key>(key));
  return s;
}

template<PARLAY_RANGE_TYPE R>
void integer_sort_inplace(R&& in) {
  internal::integer_sort_inplace(make_slice(in), [](auto x) { return x; });
}

template<PARLAY_RANGE_TYPE R, typename Key>
void integer_sort_inplace(R&& in, Key&& key) {
  internal::integer_sort_inplace(make_slice(in), std::forward<Key>(key));
}

/* -------------------- Boolean searching -------------------- */

// TODO: All of, any of, none of



}  // namespace parlay

#endif  // PARLAY_PRIMITIVES_H_
