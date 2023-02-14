
#ifndef PARLAY_PRIMITIVES_H_
#define PARLAY_PRIMITIVES_H_

#include <cassert>
#include <cstddef>
#include <cctype>

#include <algorithm>
#include <atomic>
#include <functional>
#include <optional>
#include <random>
#include <tuple>                          // IWYU pragma: keep
#include <type_traits>
#include <utility>

#include "internal/counting_sort.h"
#include "internal/integer_sort.h"
#include "internal/group_by.h"            // IWYU pragma: export
#include "internal/heap_tree.h"           // IWYU pragma: keep
#include "internal/merge.h"
#include "internal/merge_sort.h"
#include "internal/sequence_ops.h"        // IWYU pragma: export
#include "internal/sample_sort.h"

#include "delayed.h"
#include "delayed_sequence.h"
#include "monoid.h"                       // IWYU pragma: export
#include "parallel.h"
#include "random.h"
#include "range.h"
#include "sequence.h"
#include "slice.h"
#include "type_traits.h"               // IWYU pragma: keep
#include "utilities.h"

// IWYU pragma: no_include <sstream>
// IWYU pragma: no_include <bits/utility.h>

namespace parlay {

/* -------------------- Map and Tabulate -------------------- */
// Pull all of these from internal/sequence_ops.h

using internal::tabulate;

// Return a sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
using internal::map;

// Return a delayed sequence consisting of the elements
//   f(0), f(1), ... f(n)
using internal::delayed_tabulate;

// Return a delayed sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
//
// If r is a temporary, the delayed sequence will take
// ownership of it by moving it. If r is a reference,
// the delayed sequence will hold a reference to it, so
// r must remain alive as long as the delayed sequence.
using internal::delayed_map;

// Renamed to delayed_seq
using internal::dseq;

// Renamed to delayed_map.
using internal::dmap;

/* -------------------- Copying -------------------- */

// Copy the elements from the range in into the range out
// The range out must be at least as large as in.
template<typename R_in, typename R_out>
void copy(R_in&& in, R_out&& out) {
  static_assert(is_random_access_range_v<R_in>);
  static_assert(is_random_access_range_v<R_out>);
  static_assert(std::is_assignable_v<range_reference_type_t<R_out>, range_reference_type_t<R_in>>);
  assert(parlay::size(out) >= parlay::size(in));
  parallel_for(0, parlay::size(in), [in_it = std::begin(in), out_it = std::begin(out)](size_t i) {
    out_it[i] = in_it[i];
  });
}

/* ---------------------- Reduce ---------------------- */

// Compute the reduction of the elements of r with respect
// to m. That is, given a sequence r[0], ..., r[n-1], and
// an associative operation +, compute
//  r[0] + r[1] + ... + r[n-1]
template<typename R, typename Monoid,
         std::enable_if_t<is_monoid_v<Monoid>, int> = 0>
auto reduce(R&& r, Monoid&& m) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<R>>);
  return internal::reduce(make_slice(r), std::forward<Monoid>(m));
}

template<typename R, typename LegacyMonoid,
    std::enable_if_t<!is_monoid_v<LegacyMonoid> && is_legacy_monoid_v<LegacyMonoid>, int> = 0>
auto reduce(R&& r, LegacyMonoid m) {
  static_assert(is_random_access_range_v<R>);
  return parlay::reduce(std::forward<R>(r), legacy_monoid_adapter(std::move(m)));
}

// Compute the sum of the elements of r
template<typename R>
auto reduce(R&& r) {
  static_assert(is_random_access_range_v<R>);
  return parlay::reduce(r, parlay::plus<range_value_type_t<R>>());
}

/* ---------------------- Scans --------------------- */

template<typename R>
auto scan(R&& r) {
  static_assert(is_random_access_range_v<R>);
  return internal::scan(make_slice(r), parlay::plus<range_value_type_t<R>>());
}

template<typename R>
auto scan_inclusive(R&& r) {
  static_assert(is_random_access_range_v<R>);
  return internal::scan(make_slice(r), parlay::plus<range_value_type_t<R>>(), internal::fl_scan_inclusive).first;
}

template<typename R>
auto scan_inplace(R&& r) {
  static_assert(is_random_access_range_v<R>);
  return internal::scan_inplace(make_slice(r), parlay::plus<range_value_type_t<R>>());
}

template<typename R>
auto scan_inclusive_inplace(R&& r) {
  static_assert(is_random_access_range_v<R>);
  return internal::scan_inplace(make_slice(r), parlay::plus<range_value_type_t<R>>(), internal::fl_scan_inclusive);
}

template<typename R, typename Monoid,
         std::enable_if_t<is_monoid_v<Monoid>, int> = 0>
auto scan(R&& r, Monoid&& m) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<R>>);
  return internal::scan(make_slice(r), std::forward<Monoid>(m));
}

template<typename R, typename LegacyMonoid,
         std::enable_if_t<!is_monoid_v<LegacyMonoid> && is_legacy_monoid_v<LegacyMonoid>, int> = 0>
auto scan(R&& r, LegacyMonoid m) {
  static_assert(is_random_access_range_v<R>);
  return parlay::scan(make_slice(r), legacy_monoid_adapter(std::move(m)));
}

template<typename R, typename Monoid,
         std::enable_if_t<is_monoid_v<Monoid>, int> = 0>
auto scan_inclusive(R&& r, Monoid&& m) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<R>>);
  return internal::scan(make_slice(r), std::forward<Monoid>(m), internal::fl_scan_inclusive).first;
}

template<typename R, typename LegacyMonoid,
         std::enable_if_t<!is_monoid_v<LegacyMonoid> && is_legacy_monoid_v<LegacyMonoid>, int> = 0>
auto scan_inclusive(R&& r, LegacyMonoid m) {
  static_assert(is_random_access_range_v<R>);
  return parlay::scan_inclusive(std::forward<R>(r), legacy_monoid_adapter(std::move(m)));
}

template<typename R, typename Monoid,
         std::enable_if_t<is_monoid_v<Monoid>, int> = 0>
auto scan_inplace(R&& r, Monoid&& m) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<R>>);
  return internal::scan_inplace(make_slice(r), std::forward<Monoid>(m));
}

template<typename R, typename LegacyMonoid,
         std::enable_if_t<!is_monoid_v<LegacyMonoid> && is_legacy_monoid_v<LegacyMonoid>, int> = 0>
auto scan_inplace(R&& r, LegacyMonoid m) {
  static_assert(is_random_access_range_v<R>);
  return parlay::scan_inplace(std::forward<R>(r), legacy_monoid_adapter(std::move(m)));
}

template<typename R, typename Monoid,
         std::enable_if_t<is_monoid_v<Monoid>, int> = 0>
auto scan_inclusive_inplace(R&& r, Monoid&& m) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<R>>);
  return internal::scan_inplace(make_slice(r), std::forward<Monoid>(m), internal::fl_scan_inclusive);
}

template<typename R, typename LegacyMonoid,
         std::enable_if_t<!is_monoid_v<LegacyMonoid> && is_legacy_monoid_v<LegacyMonoid>, int> = 0>
auto scan_inclusive_inplace(R&& r, LegacyMonoid m) {
  static_assert(is_random_access_range_v<R>);
  return parlay::scan_inclusive_inplace(std::forward<R>(r), legacy_monoid_adapter(std::move(m)));
}

/* ----------------------- Pack ----------------------- */

template<typename R, typename BoolSeq>
auto pack(R&& r, BoolSeq&& b) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_random_access_range_v<BoolSeq>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolSeq>, bool>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::pack(make_slice(r), make_slice(b));
}

template<typename R_in, typename BoolSeq, typename R_out>
[[deprecated("Use pack_into_uninitialized instead.")]]
auto pack_into(R_in&& in, BoolSeq&& b, R_out&& out) {
  static_assert(is_random_access_range_v<R_in>);
  static_assert(is_random_access_range_v<R_out>);
  static_assert(is_random_access_range_v<BoolSeq>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolSeq>, bool>);
  static_assert(std::is_constructible_v<std::remove_reference_t<range_reference_type_t<R_out>>,
                                        range_reference_type_t<R_in>>);
  return internal::pack_out(make_slice(in), b, make_slice(out));
}

template<typename R_in, typename BoolSeq, typename R_out>
auto pack_into_uninitialized(R_in&& in, BoolSeq&& b, R_out&& out) {
  static_assert(is_random_access_range_v<R_in>);
  static_assert(is_random_access_range_v<R_out>);
  static_assert(is_random_access_range_v<BoolSeq>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolSeq>, bool>);
  static_assert(std::is_constructible_v<std::remove_reference_t<range_reference_type_t<R_out>>,
                                        range_reference_type_t<R_in>>);
  return internal::pack_out(make_slice(in), b, make_slice(out));
}

// Return a sequence consisting of the indices such that
// b[i] is true, or is implicitly convertible to true.
template<typename BoolSeq>
auto pack_index(BoolSeq&& b) {
  static_assert(is_random_access_range_v<BoolSeq>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolSeq>, bool>);
  return internal::pack_index<size_t>(make_slice(b));
}

// Return a sequence consisting of the indices such that
// b[i] is true, or is implicitly convertible to true.
template<typename IndexType, typename BoolSeq>
auto pack_index(BoolSeq&& b) {
  static_assert(is_random_access_range_v<BoolSeq>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolSeq>, bool>);
  return internal::pack_index<IndexType>(make_slice(b));
}

/* ----------------------- Filter --------------------- */

// Return a sequence consisting of the elements x of
// r such that f(x) == true.
template<typename R, typename UnaryPred>
auto filter(R&& r, UnaryPred&& f) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::filter(make_slice(r), std::forward<UnaryPred>(f));
}

template<typename R_in, typename R_out, typename UnaryPred>
[[deprecated("Use filter_into_uninitialized instead.")]]
auto filter_into(R_in&& in, R_out&& out, UnaryPred&& f) {
  static_assert(is_random_access_range_v<R_in>);
  static_assert(is_random_access_range_v<R_out>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R_in>>);
  static_assert(std::is_constructible_v<std::remove_reference_t<range_reference_type_t<R_out>>,
                                        range_reference_type_t<R_in>>);
  return internal::filter_out(make_slice(in), make_slice(out), std::forward<UnaryPred>(f));
}

template<typename R_in, typename R_out, typename UnaryPred>
auto filter_into_uninitialized(R_in&& in, R_out&& out, UnaryPred&& f) {
  static_assert(is_random_access_range_v<R_in>);
  static_assert(is_random_access_range_v<R_out>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R_in>>);
  static_assert(std::is_constructible_v<std::remove_reference_t<range_reference_type_t<R_out>>,
                                        range_reference_type_t<R_in>>);
  return internal::filter_out(make_slice(in), make_slice(out), std::forward<UnaryPred>(f));
}

/* ----------------------- Partition --------------------- */

// TODO: Partition

// TODO: nth element

// TODO: partial sort? maybe

/* ----------------------- Merging --------------------- */

template<typename R1, typename R2, typename BinaryPred>
auto merge(R1&& r1, R2&& r2, BinaryPred&& pred) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_same_v<range_value_type_t<R1>, range_value_type_t<R2>>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  static_assert(std::is_constructible_v<range_value_type_t<R1>, range_reference_type_t<R1>>);
  static_assert(std::is_constructible_v<range_value_type_t<R1>, range_reference_type_t<R2>>);
  return internal::merge(make_slice(r1), make_slice(r2), std::forward<BinaryPred>(pred));
}

template<typename R1, typename R2>
auto merge(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::merge(r1, r2, std::less<>());
}

/* -------------------- General Sorting -------------------- */

// Sort the given sequence and return the sorted sequence
template<typename R>
[[nodiscard]] auto sort(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::sample_sort(make_slice(in), std::less<>());
}

// Sort the given sequence with respect to the given
// comparison operation. Elements are sorted such that
// for an element x < y in the sequence, comp(x,y) = true
template<typename R, typename Compare>
[[nodiscard]] auto sort(R&& in, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::sample_sort(make_slice(in), std::forward<Compare>(comp));
}

template<typename R>
[[nodiscard]] auto stable_sort(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::sample_sort(make_slice(in), std::less<>{}, true);
}

template<typename R, typename Compare>
[[nodiscard]] auto stable_sort(R&& in, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::sample_sort(make_slice(in), std::forward<Compare>(comp), true);
}

template<typename R, typename Compare>
void sort_inplace(R&& in, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  internal::sample_sort_inplace(make_slice(in), std::forward<Compare>(comp));
}

template<typename R>
void sort_inplace(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  sort_inplace(std::forward<R>(in), std::less<>{});
}

template<typename R, typename Compare>
void stable_sort_inplace(R&& in, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  internal::merge_sort_inplace(make_slice(in), std::forward<Compare>(comp));
}

template<typename R>
void stable_sort_inplace(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  stable_sort_inplace(std::forward<R>(in), std::less<>{});
}

/* -------------------- Integer Sorting -------------------- */

template<typename R>
[[nodiscard]] auto integer_sort(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_integral_v<range_value_type_t<R>>);
  static_assert(std::is_unsigned_v<range_value_type_t<R>>);
  return internal::integer_sort(make_slice(in), [](auto x) { return x; }); 
}

template<typename R, typename Key>
[[nodiscard]] auto integer_sort(R&& in, Key&& key) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<R>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<R>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::integer_sort(make_slice(in), std::forward<Key>(key));
}

template<typename R>
void integer_sort_inplace(R&& in) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_integral_v<range_value_type_t<R>>);
  static_assert(std::is_unsigned_v<range_value_type_t<R>>);
  internal::integer_sort_inplace(make_slice(in), [](auto x) { return x; });
}

template<typename R, typename Key>
void integer_sort_inplace(R&& in, Key&& key) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<R>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<R>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  internal::integer_sort_inplace(make_slice(in), std::forward<Key>(key));
}

template<typename R, typename Key>
[[nodiscard]] auto stable_integer_sort(R&& in, Key&& key) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<R>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<R>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return internal::integer_sort(make_slice(in), std::forward<Key>(key));
}

template<typename R, typename Key>
void stable_integer_sort_inplace(R&& in, Key&& key) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<R>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<R>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  internal::integer_sort_inplace(make_slice(in), std::forward<Key>(key));
}

/* -------------------- Counting Sort -------------------- */

template<typename Range>
[[nodiscard]] auto counting_sort(Range&& in, size_t num_buckets) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_integral_v<range_value_type_t<Range>>);
  static_assert(std::is_unsigned_v<range_value_type_t<Range>>);
  return internal::count_sort(make_slice(in), make_slice(in), num_buckets);
}

template<typename Range, typename Key>
[[nodiscard]] auto counting_sort(Range&& in, size_t num_buckets, Key&& key) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<Range>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<Range>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  auto keys = internal::delayed_map(in, std::forward<Key>(key));
  return internal::count_sort(make_slice(in), keys, num_buckets);
}

template<typename Range>
auto counting_sort_inplace(Range&& in, size_t num_buckets) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_integral_v<range_value_type_t<Range>>);
  static_assert(std::is_unsigned_v<range_value_type_t<Range>>);
  return internal::count_sort_inplace(make_slice(in), make_slice(in), num_buckets);
}

template<typename Range, typename Key>
auto counting_sort_inplace(Range&& in, size_t num_buckets, Key&& key) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_invocable_v<Key, range_reference_type_t<Range>>);
  using key_type = std::invoke_result_t<Key, range_reference_type_t<Range>>;
  static_assert(std::is_integral_v<key_type>);
  static_assert(std::is_unsigned_v<key_type>);
  auto keys = internal::delayed_map(in, std::forward<Key>(key));
  return internal::count_sort_inplace(make_slice(in), keys, num_buckets);
}

template<typename Range>
[[nodiscard]] auto counting_sort_by_keys(Range&& in, size_t num_buckets) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(is_pair_v<range_reference_type_t<Range>>);
  static_assert(std::is_integral_v<std::tuple_element_t<0, range_reference_type_t<Range>>>);
  static_assert(std::is_unsigned_v<std::tuple_element_t<0, range_reference_type_t<Range>>>);
  auto values = delayed::values_view(in);
  return internal::count_sort(make_slice(values), delayed::keys_view(in), num_buckets);
}

/* -------------------- Internal count and find -------------------- */

namespace internal {

template <typename IntegerPred>
size_t count_if_index(size_t n, IntegerPred&& p) {
  static_assert(std::is_invocable_r_v<bool, IntegerPred, size_t>);
  auto BS = delayed_tabulate(n, [&](size_t i) -> size_t {
      return static_cast<bool>(p(i)); });
  return parlay::reduce(make_slice(BS));
}

template <typename IntegerPred>
size_t find_if_index(size_t n, IntegerPred&& p, size_t granularity = 1000) {
  static_assert(std::is_invocable_r_v<bool, IntegerPred, size_t>);
  size_t i;
  for (i = 0; i < (std::min)(granularity, n); i++)
    if (p(i)) return i;
  if (i == n) return n;
  size_t start = granularity;
  size_t block_size = 2 * granularity;
  std::atomic<size_t> result(n);
  while (start < n) {
    size_t end = (std::min)(n, start + block_size);
    parallel_for(start, end, [&](size_t j) {
      if (p(j)) write_min(&result, j, std::less<>());
    }, granularity);
    if (auto res = result.load(); res < n) return res;
    start += block_size;
    block_size *= 2;
  }
  return n;
}

}  // namespace internal

/* -------------------- For each -------------------- */

template <typename R, typename UnaryFunction>
void for_each(R&& r , UnaryFunction&& f) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<UnaryFunction, range_reference_type_t<R>>);
  parallel_for(0, parlay::size(r),
    [&f, it = std::begin(r)](size_t i) { f(it[i]); });
}

/* -------------------- Counting -------------------- */

template <typename R, typename UnaryPredicate>
size_t count_if(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return internal::count_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <typename R, class T>
size_t count(R&& r, const T& value) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R>, const T&>);
  return internal::count_if_index(parlay::size(r),
     [&] (size_t i) { return r[i] == value; });
}

/* -------------------- Boolean searching -------------------- */

template <typename R, typename UnaryPredicate>
bool all_of(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return parlay::count_if(r, std::forward<UnaryPredicate>(p)) == parlay::size(r);
}

template <typename R, typename UnaryPredicate>
bool any_of(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return parlay::count_if(r, std::forward<UnaryPredicate>(p)) > 1;
}

template <typename R, typename UnaryPredicate>
bool none_of(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return parlay::count_if(r, std::forward<UnaryPredicate>(p)) == 0;
}

/* -------------------- Finding -------------------- */

template <typename R, typename UnaryPredicate>
auto find_if(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return std::begin(r) + internal::find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <typename R, typename T>
auto find(R&& r, const T& value) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R>, const T&>);
  return parlay::find_if(r, [&](auto&& x) { return std::forward<decltype(x)>(x) == value; });
}

template <typename R, typename UnaryPredicate>
auto find_if_not(R&& r, UnaryPredicate&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<R>>);
  return std::begin(r) + internal::find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return !p(it[i]); });
}

template <typename R1, typename R2, typename BinaryPredicate>
auto find_first_of(R1&& r1, R2&& r2, BinaryPredicate&& p) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, BinaryPredicate, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return std::begin(r1) + internal::find_if_index(parlay::size(r1), [&, it = std::begin(r1)] (size_t i) {
    return find_if(r2, [&, it](auto&& x) { return p(std::forward<decltype(x)>(x), it[i]); }) != std::end(r2);
  });
}

template <typename R1, typename R2>
auto find_first_of(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::find_first_of(std::forward<R1>(r1), std::forward<R2>(r2), std::equal_to<>());
}

/* -------------------- Adjacent Finding -------------------- */

template <typename R, typename BinaryPred>
auto adjacent_find(R&& r, BinaryPred&& p) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R>, range_reference_type_t<R>>);
  auto idx = internal::find_if_index(parlay::size(r) - 1,
                 [&p, it = std::begin(r)](size_t i) {
                   return p(it[i], it[i + 1]); });
  if (idx == parlay::size(r) - 1) return std::end(r);
  else return std::begin(r) + idx;
}

template <typename R>
auto adjacent_find(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R>>);
  return parlay::adjacent_find(std::forward<R>(r), std::equal_to<>());
}

/* ----------------------- Mismatch ----------------------- */

template <typename R1, typename R2, typename BinaryPred>
auto mismatch(R1&& r1, R2&& r2, BinaryPred p) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  auto d = internal::find_if_index(std::min(parlay::size(r1), parlay::size(r2)),
    [&p, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i) {
      return !p(it1[i], it2[i]); });
  return std::make_pair(std::begin(r1) + d, std::begin(r2) + d);
}

template <typename R1, typename R2>
auto mismatch(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::mismatch(std::forward<R1>(r1), std::forward<R2>(r2), std::equal_to<>());
}

/* ----------------------- Pattern search ----------------------- */

template <typename R1, typename R2, typename BinaryPred>
auto search(R1&& r1, R2&& r2, BinaryPred pred) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return std::begin(r1) + internal::find_if_index(parlay::size(r1), [&](size_t i) {
    if (i + parlay::size(r2) > parlay::size(r1)) return false;
    size_t j;
    for (j = 0; j < parlay::size(r2); j++)
      if (!pred(r1[i + j], r2[j])) break;
    return (j == parlay::size(r2));
  });
}

template <typename R1, typename R2>
auto search(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::search(r1, r2, std::equal_to<>());
}

template <typename R1, typename R2, typename BinaryPred>
auto find_end(R1&& r1, R2&& r2, BinaryPred&& p) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  size_t n1 = parlay::size(r1);
  size_t n2 = parlay::size(r2);

  // According to the C++ standard, std::find_end should return the end iterator if the pattern is empty
  if (n2 == 0) return std::end(r1);

  size_t idx = internal::find_if_index(n1 - n2 + 1, [&](size_t i) {
    size_t j;
    for (j = 0; j < n2; j++)
      if (!p(r1[(n1 - i - n2) + j], r2[j])) break;
    return (j == parlay::size(r2));
  });

  if (idx == n1 - n2 + 1) return std::end(r1);
  else return std::begin(r1) + n1 - idx - n2;
}

template <typename R1, typename R2>
auto find_end(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::find_end(std::forward<R1>(r1), std::forward<R2>(r2), std::equal_to<>());
}

/* ------------------------- Equal ------------------------- */

template <typename R1, typename R2, class BinaryPred>
bool equal(R1&& r1, R2&& r2, BinaryPred&& p) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, BinaryPred, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::size(r1) == parlay::size(r2) &&
    internal::find_if_index(parlay::size(r1),
      [&p, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i)
        { return !p(it1[i], it2[i]); }) == parlay::size(r1);
}

template <typename R1, typename R2>
bool equal(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::equal(r1, r2, std::equal_to<>());
}

/* ---------------------- Lex compare ---------------------- */

template <typename R1, typename R2, class Compare>
bool lexicographical_compare(R1&& r1, R2&& r2, Compare&& less) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R1>, range_reference_type_t<R2>>);
  size_t m = (std::min)(parlay::size(r1), parlay::size(r2));
  size_t i = internal::find_if_index(m, [&less, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i)
      { return less(it1[i], it2[i]) || less(it2[i], it1[i]); });
  return (i < m) ? (less(std::begin(r1)[i], std::begin(r2)[i]))
    : (parlay::size(r1) < parlay::size(r2));
}

template <typename R1, typename R2>
inline bool lexicographical_compare(R1&& r1, R2&& r2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R1>, range_reference_type_t<R2>>);
  return parlay::lexicographical_compare(std::forward<R1>(r1), std::forward<R2>(r2), std::less<>{});}

template <typename T, typename Alloc, bool EnableSSO>
inline bool operator<(const sequence<T,Alloc,EnableSSO> &a, const sequence<T,Alloc,EnableSSO> &b) {
  if (a.size() > 1000) 
    return lexicographical_compare(a, b);
  auto sa = a.begin();
  auto sb = b.begin();
  auto ea = sa + (std::min)(a.size(),b.size());
  while (sa < ea && *sa == *sb) {sa++; sb++;}
  return sa == ea ? (a.size() < b.size()) : *sa < *sb;
};


/* -------------------- Remove duplicates -------------------- */

template <typename R, typename BinaryPredicate>
auto unique(R&& r, BinaryPredicate&& eq) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, BinaryPredicate, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  auto b = internal::delayed_tabulate(parlay::size(r), [&eq, it = std::begin(r)](size_t i)
      { return (i == 0) || !eq(it[i], it[i - 1]); });
  return parlay::pack(r, b);
}

template<typename R>
auto unique(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return parlay::unique(r, std::equal_to<>());
}


/* -------------------- Min and max -------------------- */

// needs to return location, and take comparison
template <typename R, typename Compare>
auto min_element(R&& r, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  size_t n = parlay::size(r);
  if (n == 0) return std::begin(r);
  auto SS = delayed_seq<size_t>(n, [&](size_t i) { return i; });
  auto f = [&comp, it = std::begin(r)](size_t l, size_t r)
    { return (!comp(it[r], it[l]) ? l : r); };
  return std::begin(r) + internal::reduce(make_slice(SS), make_monoid(f, (size_t)parlay::size(r)));
}

template <typename R>
auto min_element(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  return parlay::min_element(std::forward<R>(r), std::less<>());
}

template <typename R, typename Compare>
auto max_element(R&& r, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  return parlay::min_element(std::forward<R>(r), [&](auto&& a, auto&& b) {
    return comp(std::forward<decltype(b)>(b), std::forward<decltype(a)>(a)); });
}

template <typename R>
auto max_element(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  return parlay::max_element(std::forward<R>(r), std::less<>());
}

template <typename R, typename Compare>
auto minmax_element(R&& r, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  size_t n = parlay::size(r);
  if (n == 0) return std::make_pair(std::begin(r), std::begin(r));
  auto SS = delayed_seq<std::pair<size_t, size_t>>(parlay::size(r),
    [&](size_t i) { return std::make_pair(i, i); });
  auto f = [&comp, it = std::begin(r)](const auto& l, const auto& r) {
    return (std::make_pair(!comp(it[r.first], it[l.first]) ? l.first : r.first,
              !comp(it[l.second], it[r.second]) ? l.second : r.second));
  };
  auto ds = internal::reduce(make_slice(SS), make_monoid(f, std::make_pair(n, n)));
  return std::make_pair(std::begin(r) + ds.first, std::begin(r) + ds.second);
}

template <typename R>
auto minmax_element(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<range_reference_type_t<R>>);
  return parlay::minmax_element(std::forward<R>(r), std::less<>());
}

/* -------------------- Permutations -------------------- */

template <typename R>
auto reverse(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  auto n = parlay::size(r);
  return internal::tabulate<range_value_type_t<R>>(n, [n, it = std::begin(r)](size_t i) {
    return it[n - i - 1];
  });
}

template <typename R>
auto reverse_inplace(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_swappable_v<range_reference_type_t<R>>);
  auto n = parlay::size(r);
  parallel_for(0, n/2, [n, it = std::begin(r)] (size_t i) {
    using std::swap;
    swap(it[i], it[n - i - 1]);
  });
}

template <typename R>
auto rotate(const R& r, size_t t) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  size_t n = parlay::size(r);
  return internal::tabulate<range_value_type_t<R>>(parlay::size(r),
    [n, t, it = std::begin(r)](size_t i) {
      size_t j = (i + t < n) ? (i + t) : i + t - n;
      return it[j];
    });
}

/* -------------------- Is sorted? -------------------- */

template <typename R, typename Compare>
bool is_sorted(R&& r, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare,range_reference_type_t<R>, range_reference_type_t<R>>);
  if (parlay::size(r) == 0) return true;
  auto B = delayed_seq<bool>(parlay::size(r) - 1, [&comp, it = std::begin(r)](size_t i)
      { return comp(it[i + 1], it[i]); });
  return (internal::reduce(make_slice(B), parlay::make_monoid(std::logical_or<>(), false)) == 0);
}

template <typename R>
bool is_sorted(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<R>);
  return parlay::is_sorted(std::forward<R>(r), std::less<>());
}

template <typename R, typename Compare>
auto is_sorted_until(R&& r, Compare&& comp) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare,range_reference_type_t<R>, range_reference_type_t<R>>);
  if (parlay::size(r) == 0) return std::begin(r);
  return std::begin(r) + internal::find_if_index(parlay::size(r) - 1,
    [&comp, it = std::begin(r)](size_t i) { return comp(it[i + 1], it[i]); }) + 1;
}

template <typename R>
auto is_sorted_until(R& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_less_than_comparable_v<R>);
  return parlay::is_sorted_until(std::forward<R>(r), std::less<>());
}

/* -------------------- Is partitioned? -------------------- */

template <typename R, typename UnaryPred>
bool is_partitioned(R&& r, UnaryPred&& f) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R>>);
  auto n = parlay::size(r);
  auto d = internal::find_if_index(n, [&f, it = std::begin(r)](size_t i) {
    return !f(it[i]);
  });
  if (d == n) return true;
  auto d2 = internal::find_if_index(n - d - 1, [&f, it = std::begin(r) + d + 1](size_t i) {
    return f(it[i]);
  });
  return (d2 == n - d - 1);
}

/* -------------------- Remove -------------------- */

template <typename R, typename UnaryPred>
auto remove_if(R&& r, UnaryPred&& pred) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  auto flags = delayed_seq<bool>(parlay::size(r),
    [&pred, it = std::begin(r)](size_t i) { return !pred(it[i]); });
  return internal::pack(r, flags);
}

template<typename R, typename T>
auto remove(R&& r, const T& v) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_equality_comparable_v<range_reference_type_t<R>, const T&>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return remove_if(r, [&](auto&& x) { return std::forward<decltype(x)>(x) == v; });
}

/* -------------------- Iota -------------------- */

template <typename Index>
auto iota(Index n) {
  static_assert(std::is_integral_v<Index>);
  return delayed_tabulate<Index, Index>(n, [](size_t i) -> Index { return i; });
}

/* -------------------- Flatten -------------------- */

template <typename R>
auto flatten(R&& r) {
  static_assert(is_random_access_range_v<R>);
  static_assert(is_random_access_range_v<range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<range_reference_type_t<R>>,
                                        range_reference_type_t<range_reference_type_t<R>>>);

  using T = range_value_type_t<range_reference_type_t<R>>;
  auto offsets = tabulate(parlay::size(r), [it = std::begin(r)](size_t i)
                          { return parlay::size(it[i]); });
  size_t len = internal::scan_inplace(make_slice(offsets), plus<size_t>());
  auto res = sequence<T>::uninitialized(len);
  parallel_for(0, parlay::size(r), [&, it = std::begin(r)](size_t i) {
    auto dit = std::begin(res)+offsets[i];
    auto sit = std::begin(it[i]);
    parallel_for(0, parlay::size(it[i]), [=] (size_t j) {
      assign_uninitialized(*(dit+j), *(sit+j));
    }, 1000);
  });
  return res;
}


template <typename T>
auto flatten(sequence<sequence<T>>&& r) {
  static_assert(std::is_move_constructible_v<T>);
  auto offsets = tabulate(parlay::size(r),
    [it = std::begin(r)](size_t i) { return parlay::size(it[i]); });
  size_t len = internal::scan_inplace(make_slice(offsets), plus<size_t>());
  auto res = sequence<T>::uninitialized(len);
  parallel_for(0, parlay::size(r), [&, it = std::begin(r)](size_t i) {
    uninitialized_relocate_n(std::begin(res)+offsets[i], std::begin(it[i]), it[i].size());
    clear_relocated(it[i]);
  });
  r.clear();
  return res;
}

/* -------------------- Tokens and split -------------------- */

// Return true if the given character is considered whitespace.
// Whitespace characters are ' ', '\f', '\n', '\r'. '\t'. '\v'.
bool inline is_whitespace(unsigned char c) {
  return std::isspace(c);
}

namespace internal {
template<typename R, typename UnaryOp, typename UnaryPred = decltype(is_whitespace)>
auto map_tokens_small(R&& r, UnaryOp&& f, UnaryPred&& is_space = is_whitespace) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<UnaryOp, decltype(make_slice(r))>);

  using f_return_type = std::invoke_result_t<UnaryOp, decltype(make_slice(r))>;

  auto S = make_slice(r);
  size_t n = S.size();

  if (n == 0) {
    if constexpr (std::is_void_v<f_return_type>) return;
    else return sequence<f_return_type>();
  }

  // sequence<long> Locations = pack_index<long>(Flags);
  sequence<size_t> Locations = internal::delayed::to_sequence(
                               internal::delayed::filter_op(
                                 parlay::iota<size_t>(n + 1),
                                 [&](size_t i) -> std::optional<size_t> {
    if ((i == 0) ? !is_space(S[0]) : (i == n) ? !is_space(S[n - 1]) : is_space(S[i - 1]) != is_space(S[i]))
      return std::make_optional(i);
    else
      return std::nullopt;
  }));

  // If f does not return anything, just apply f
  // to each token and don't return anything
  if constexpr (std::is_void_v<f_return_type>) {
    parallel_for(0, Locations.size() / 2, [&](size_t i) {
      f(S.cut(Locations[2 * i], Locations[2 * i + 1]));
    });
  }
  // If f does return something, apply f to each token
  // and return a sequence of the resulting values
  else {
    return tabulate(Locations.size() / 2, [&](size_t i) {
      return f(S.cut(Locations[2 * i], Locations[2 * i + 1]));
    });
  }
}
}

// Apply the given function f to each token of the given range.
//
// The tokens are the longest contiguous subsequences of non space characters.
// where spaces are define by the unary predicate is_space. By default, is_space
// correponds to std::isspace, which is true for ' ', '\f', '\n', '\r'. '\t'. '\v'
//
// If f has no return value (i.e. void), this function returns nothing. Otherwise,
// this function returns a sequence consisting of the returned values of f for
// each token.
template<typename R, typename UnaryOp, typename UnaryPred = decltype(is_whitespace)>
auto map_tokens(R&& r, UnaryOp&& f, UnaryPred&& is_space = is_whitespace) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<UnaryOp, decltype(make_slice(r))>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<R>>);

  using f_return_type = decltype(f(make_slice(r)));
  auto A = make_slice(r);
  using ipair = std::pair<long, long>;
  size_t n = A.size();

  if (n == 0) {
    if constexpr (std::is_void_v<f_return_type>) return;
    else return sequence<f_return_type>();
  }

  auto is_start = [&](size_t i) { return ((i == 0) || is_space(A[i - 1])) && (i != n) && !(is_space(A[i])); };
  auto is_end = [&](size_t i) { return ((i == n) || (is_space(A[i]))) && (i != 0) && !is_space(A[i - 1]); };

  // associative combining function
  // first = # of starts, second = index of last start
  auto g = [](ipair a, ipair b) { return (b.first == 0) ? a : ipair(a.first + b.first, b.second); };

  auto in = internal::delayed_tabulate(n + 1, [&](size_t i) -> ipair {
    return is_start(i) ? ipair(1, i) : ipair(0, 0); });
  auto [offsets, sum] = internal::delayed::scan(in, g, ipair(0, 0));

  auto z = internal::delayed::zip(offsets, parlay::iota(n + 1));

  auto start = std::begin(r);

  if constexpr (std::is_same_v<f_return_type, void>)
    internal::delayed::apply(z, [&](auto&& x) {
      if (is_end(std::get<1>(x))) f(make_slice(std::get<0>(x).second + start, std::get<1>(x) + start));
    });
  else {
    auto result = sequence<f_return_type>::uninitialized(sum.first);
    internal::delayed::apply(z, [&](auto&& x) {
      if (is_end(std::get<1>(x)))
        assign_uninitialized(result[std::get<0>(x).first - 1],
                             f(make_slice(start + std::get<0>(x).second, start + std::get<1>(x))));
    });
    return result;
  }
}

// Returns a sequence of tokens, each represented as a sequence of chars.
// The tokens are the longest contiguous subsequences of non space characters.
// where spaces are define by the unary predicate is_space. By default, is_space
// correponds to std::isspace, which is true for ' ', '\f', '\n', '\r'. '\t'. '\v'
template <typename Range, typename UnaryPred = decltype(is_whitespace)>
sequence<parlay::chars> tokens(Range&& R, UnaryPred&& is_space = is_whitespace) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_convertible_v<range_reference_type_t<Range>, char>);
  static_assert(std::is_invocable_r_v<bool, UnaryPred, range_reference_type_t<Range>>);
  auto to_token = [](auto&& x) { return to_short_sequence(std::forward<decltype(x)>(x)); };
  if (parlay::size(R) < 2000) return internal::map_tokens_small(R, to_token, is_space);
  else return map_tokens(R, to_token, is_space);
}

// Like split_at, but applies the given function f to each of the
// contiguous subsequences of R delimited by positions i such that
// flags[i] is (or converts to) true.
template <typename Range, typename BoolRange, typename UnaryOp>
auto map_split_at(Range&& R, BoolRange&& flags, UnaryOp&& f) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(is_random_access_range_v<BoolRange>);
  static_assert(std::is_convertible_v<range_reference_type_t<BoolRange>, bool>);
  static_assert(std::is_invocable_v<UnaryOp, decltype(make_slice(R))>);

  auto S = make_slice(R);
  auto Flags = make_slice(flags);
  size_t n = S.size();
  assert(Flags.size() == n);

  sequence<size_t> Locations = pack_index<size_t>(Flags);
  size_t m = Locations.size();

  return tabulate(m + 1, [&] (size_t i) {
    size_t start = (i==0) ? 0 : Locations[i-1] + 1;
    size_t end = (i==m) ? n : Locations[i] + 1;
    return f(S.cut(start, end)); });
}

// Partitions R into contiguous subsequences, by marking the last
// element of each subsequence with a true in flags.  There is an
// implied flag at the end.  Returns a nested sequence whose sum of
// lengths is equal to the original length.  If the last position is
// marked as true, then there will be an empty subsequence at the end.
// The length of the result is therefore always one more than the
// number of true flags.
template <typename Range, typename BoolRange>
auto split_at(Range&& R, BoolRange&& flags) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(is_random_access_range_v<BoolRange>);
  static_assert(std::is_convertible_v<range_value_type_t<BoolRange>, bool>);
  static_assert(std::is_constructible_v<range_value_type_t<Range>, range_reference_type_t<Range>>);
  if constexpr (std::is_same<range_value_type_t<Range>, char>::value)
    return parlay::map_split_at(R, flags, [](auto&& x) { return to_short_sequence(std::forward<decltype(x)>(x)); });
  else
    return parlay::map_split_at(R, flags, [](auto&& x) { return to_sequence(std::forward<decltype(x)>(x)); });
}


/* -------------------- Other Utilities -------------------- */

template <typename R, typename Compare = std::less<>>
auto remove_duplicates_ordered(R&& s, Compare&& less = {}) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<R>, range_reference_type_t<R>>);
  static_assert(std::is_constructible_v<range_value_type_t<R>, range_reference_type_t<R>>);
  return unique(stable_sort(s, less), [&] (auto&& a, auto&& b) {
      return !less(a,b) && !less(b,a);});
}

// returns sequence with element of same type as the first argument
template <typename R1, typename R2>
auto append(R1&& s1, R2&& s2) {
  static_assert(is_random_access_range_v<R1>);
  static_assert(is_random_access_range_v<R2>);
  using T = range_value_type_t<R1>;
  static_assert(std::is_constructible_v<T, range_reference_type_t<R1>>);
  static_assert(std::is_constructible_v<T, range_reference_type_t<R2>>);
  size_t n1 = s1.size();
  return tabulate(n1 + s2.size(), [&] (size_t i) -> T {
      return (i < n1) ? s1[i] : s2[i-n1];});
}

template<typename Range, typename UnaryOperator>
auto map_maybe(Range&& v, UnaryOperator&& p) {
  return internal::delayed::to_sequence(
    internal::delayed::filter_op(std::forward<Range>(v), std::forward<UnaryOperator>(p)));
}

template<typename... Ranges>
auto zip(Ranges&&... rs) {
  return internal::delayed::to_sequence(internal::delayed::zip(std::forward<Ranges>(rs)...));
}

// Returns a sequence consisting of the rank of each element in the input range. The rank
// of an element is the number of elements in the range that compare less than it, according
// to the binary operator compare, which defaults to less-than comparison.
template<typename size_type, typename Range, typename BinaryOperator = std::less<>>
auto rank(Range&& r, BinaryOperator&& compare = {}) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_invocable_r_v<bool, BinaryOperator, range_reference_type_t<Range>, range_reference_type_t<Range>>);
  parlay::sequence<size_type> position = parlay::to_sequence(parlay::iota<size_type>(size(r)));
  parlay::stable_sort_inplace(position, [it = std::begin(r), compare = std::forward<BinaryOperator>(compare)]
                                (auto i, auto j) { return compare(it[i], it[j]); });
  auto rank = parlay::sequence<size_type>::uninitialized(position.size());
  parallel_for(0, position.size(), [&](size_t i) {
    rank[position[i]] = i;
  });
  return rank;
}

template<typename Range, typename BinaryOperator = std::less<>>
auto rank(Range&& r, BinaryOperator&& compare = {}) {
  return parlay::rank<std::make_unsigned_t<range_difference_type_t<Range>>>(
      std::forward<Range>(r), std::forward<BinaryOperator>(compare));
}


template <typename Range, typename Compare = std::less<>>
auto kth_smallest_copy(Range&& in, size_t k, Compare&& less = {}) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_constructible_v<range_value_type_t<Range>, range_reference_type_t<Range>>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<Range>, range_reference_type_t<Range>>);

  size_t n = parlay::size(in);
  assert(k < n);
  if (n <= 1000) return parlay::sort(std::forward<Range>(in), std::forward<Compare>(less))[k];

  auto it = std::begin(in);

  // pick 31 pivots by randomly choosing 8 * 31 keys, sorting them,
  // and taking every 8th key (i.e. oversampling)
  constexpr size_t sample_size = 31;
  constexpr size_t over = 8;

  parlay::random_generator gen;
  std::uniform_int_distribution<size_t> dis(0, n-1);
  auto pivots = parlay::sort(parlay::tabulate(sample_size*over, [&] (size_t i) -> range_value_type_t<Range> {
    auto r = gen[i];
    return it[dis(r)];
  }), less);
  pivots = parlay::tabulate(sample_size,[&] (size_t i) { return pivots[i*over]; });

  // Determine which of the 32 buckets each key belongs in
  internal::heap_tree ss(pivots);
  auto ids = parlay::tabulate(n, [&] (long i) -> unsigned char {
    return ss.rank(it[i], less);});

  // Count how many in keys are each bucket
  auto sums = parlay::histogram_by_index(ids, sample_size+1);
  
  // find which bucket k belongs in, and pack the keys in that bucket into next
  auto [offsets, total] = parlay::scan(sums);
  auto id = std::upper_bound(offsets.begin(), offsets.end(), k) - offsets.begin() - 1;
  auto next = parlay::pack(in, parlay::delayed_map(ids, [=] (auto b) {return b == id;}));
  
  // recurse on much smaller set, adjusting k as needed
  return kth_smallest_copy(next, k - offsets[id], std::forward<Compare>(less));
}

template <typename Range, typename Compare = std::less<>>
auto kth_smallest(Range&& in, size_t k, Compare&& less = {}) {
  static_assert(is_random_access_range_v<Range>);
  static_assert(std::is_invocable_r_v<bool, Compare, range_reference_type_t<Range>, range_reference_type_t<Range>>);
  auto iterators = parlay::tabulate(parlay::size(in), [it = std::begin(in)](size_t i) { return it + i; });
  return kth_smallest_copy(iterators, k, [less = std::forward<Compare>(less)](auto&& it1, auto&& it2) {
    return less(*it1, *it2);
  });
}

}  // namespace parlay

#endif  // PARLAY_PRIMITIVES_H_
