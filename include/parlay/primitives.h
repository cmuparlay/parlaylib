
#ifndef PARLAY_PRIMITIVES_H_
#define PARLAY_PRIMITIVES_H_

#include <cassert>
#include <cstddef>
#include <cctype>

#include <algorithm>
#include <atomic>
#include <functional>
#include <type_traits>
#include <utility>

#include "internal/integer_sort.h"
#include "internal/merge.h"
#include "internal/merge_sort.h"
#include "internal/sequence_ops.h"     // IWYU pragma: export
#include "internal/sample_sort.h"

#include "internal/block_delayed.h"

#include "delayed_sequence.h"
#include "monoid.h"
#include "parallel.h"
#include "range.h"
#include "sequence.h"
#include "slice.h"
#include "utilities.h"

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

// Deprecated. Renamed to delayed_map.
using internal::dmap;

// Return a delayed sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
//
// If r is a temporary, the delayed sequence will take
// ownership of it by moving it. If r is a reference,
// the delayed sequence will hold a reference to it, so
// r must remain alive as long as the delayed sequence.
using internal::delayed_map;


/* -------------------- Copying -------------------- */

// Copy the elements from the range in into the range out
// The range out must be at least as large as in.
template<PARLAY_RANGE_TYPE R_in, PARLAY_RANGE_TYPE R_out>
void copy(const R_in& in, R_out&& out) {
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
  return parlay::reduce(r, addm<value_type>());
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
    internal::fl_scan_inclusive).first;
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
    internal::fl_scan_inclusive).first;
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan_inplace(R&& r, Monoid&& m) {
  return internal::scan_inplace(make_slice(r), std::forward<Monoid>(m));
}

template<PARLAY_RANGE_TYPE R, typename Monoid>
auto scan_inclusive_inplace(R&& r, Monoid&& m) {
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
auto pack_into(const R_in& in, const BoolSeq& b, R_out&& out) {
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

// TODO: nth element

/* ----------------------- Merging --------------------- */

template<PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto merge(const R1& r1, const R2& r2, BinaryPred pred) {
  static_assert(std::is_same_v<range_value_type_t<R1>, range_value_type_t<R2>>,
    "The two ranges must have the same underlying type");
  return internal::merge(make_slice(r1), make_slice(r2), pred);
}

template<PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto merge(const R1& r1, const R2& r2) {
  auto comp = [](const auto& x, const auto& y) {
    return x < y;
  };
  return merge(r1, r2, comp);
}

/* ----------------------- Histograms --------------------- */

// Now in internal::collect_reduce.h
// Compute a histogram of the values of A, with m buckets.
// template<PARLAY_RANGE_TYPE R, typename Integer_>
//  auto histogram(const R& A, Integer_ m) {
//   return internal::histogram(make_slice(A), m);
//}

/* -------------------- General Sorting -------------------- */

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
  internal::sample_sort_inplace(make_slice(in), std::forward<Compare>(comp));
}

template<PARLAY_RANGE_TYPE R>
void sort_inplace(R&& in) {
  using value_type = range_value_type_t<R>;
  sort_inplace(std::forward<R>(in), std::less<value_type>{});
}

template<PARLAY_RANGE_TYPE R, typename Compare>
void stable_sort_inplace(R&& in, Compare&& comp) {
  internal::merge_sort_inplace(make_slice(in), std::forward<Compare>(comp));
}

template<PARLAY_RANGE_TYPE R>
void stable_sort_inplace(R&& in) {
  using value_type = range_value_type_t<R>;
  stable_sort_inplace(std::forward<R>(in), std::less<value_type>{});
}



/* -------------------- Integer Sorting -------------------- */

// Note: There is currently no stable integer sort.

template<PARLAY_RANGE_TYPE R>
auto integer_sort(const R& in) {
  static_assert(std::is_integral_v<std::remove_reference_t<decltype(*in.begin())>>);
  static_assert(std::is_unsigned_v<std::remove_reference_t<decltype(*in.begin())>>);
  return internal::integer_sort(make_slice(in), [](auto x) { return x; }); 
}

template<PARLAY_RANGE_TYPE R, typename Key>
auto integer_sort(const R& in, Key&& key) {
  static_assert(std::is_integral_v<std::remove_reference_t<decltype(key(*in.begin()))>>);
  static_assert(std::is_unsigned_v<std::remove_reference_t<decltype(key(*in.begin()))>>);
  return internal::integer_sort(make_slice(in), std::forward<Key>(key));
}

template<PARLAY_RANGE_TYPE R>
void integer_sort_inplace(R&& in) {
  static_assert(std::is_integral_v<std::remove_reference_t<decltype(*in.begin())>>);
  static_assert(std::is_unsigned_v<std::remove_reference_t<decltype(*in.begin())>>);
  internal::integer_sort_inplace(make_slice(in), [](auto x) { return x; });
}

template<PARLAY_RANGE_TYPE R, typename Key>
void integer_sort_inplace(R&& in, Key&& key) {
  static_assert(std::is_integral_v<std::remove_reference_t<decltype(key(*in.begin()))>>);
  static_assert(std::is_unsigned_v<std::remove_reference_t<decltype(key(*in.begin()))>>);
  internal::integer_sort_inplace(make_slice(in), std::forward<Key>(key));
}

/* -------------------- Internal count and find -------------------- */

namespace internal {

template <typename IntegerPred>
size_t count_if_index(size_t n, IntegerPred p) {
  auto BS = delayed_tabulate(n, [&](size_t i) -> size_t {
      return (bool) p(i); });
  return reduce(make_slice(BS));
}

template <typename IntegerPred>
size_t find_if_index(size_t n, IntegerPred p, size_t granularity = 1000) {
  size_t i;
  for (i = 0; i < (std::min)(granularity, n); i++)
    if (p(i)) return i;
  if (i == n) return n;
  size_t start = granularity;
  size_t block_size = 2 * granularity;
  std::atomic<size_t> result(n);
  while (start < n) {
    size_t end = (std::min)(n, start + block_size);
    parallel_for(start, end,
                 [&](size_t j) {
                   if (p(j)) write_min(&result, j, std::less<size_t>());
                 },
                 granularity);
    if (auto res = result.load(); res < n) return res;
    start += block_size;
    block_size *= 2;
  }
  return n;
}

}  // namespace internal

/* -------------------- Semisorting -------------------- */

// TODO: Semisort / group by


/* -------------------- For each -------------------- */

template <PARLAY_RANGE_TYPE R, typename UnaryFunction>
void for_each(R&& r , UnaryFunction f) {
  parallel_for(0, parlay::size(r),
    [&f, it = std::begin(r)](size_t i) { f(it[i]); });
}

/* -------------------- Counting -------------------- */

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
size_t count_if(const R& r, UnaryPredicate p) {
  return internal::count_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <PARLAY_RANGE_TYPE R, class T>
size_t count(const R& r, const T& value) {
  return internal::count_if_index(parlay::size(r),
     [&] (size_t i) { return r[i] == value; });
}

/* -------------------- Boolean searching -------------------- */

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
bool all_of(const R& r, UnaryPredicate p) {
  return count_if(r, p) == parlay::size(r);
}

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
bool any_of(const R& r, UnaryPredicate p) {
  return count_if(r, p) > 1;
}

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
bool none_of(const R& r, UnaryPredicate p) {
  return count_if(r, p) == 0;
}

/* -------------------- Finding -------------------- */

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
auto find_if(R&& r, UnaryPredicate p) {
  return std::begin(r) + internal::find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <PARLAY_RANGE_TYPE R, typename T>
auto find(R&& r, T const &value) {
  return find_if(r, [&](const auto& x) { return x == value; });
}

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
auto find_if_not(R&& r, UnaryPredicate p) {
  return std::begin(r) + internal::find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return !p(it[i]); });
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPredicate>
auto find_first_of(R1&& r1, const R2& r2, BinaryPredicate p) {
  return std::begin(r1) + internal::find_if_index(parlay::size(r1), [&] (size_t i) {
    size_t j;
    for (j = 0; j < parlay::size(r2); j++)
      if (p(std::begin(r1)[i], std::begin(r2)[j])) break;
    return (j < parlay::size(r2));
   });
}

/* ----------------------- Find end ----------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto find_end(R1&& r1, const R2& r2, BinaryPred p) {
  size_t n1 = parlay::size(r1);
  size_t n2 = parlay::size(r2);
  size_t idx = internal::find_if_index(parlay::size(r1) - parlay::size(r2) + 1, [&](size_t i) {
    size_t j;
    for (j = 0; j < n2; j++)
      if (!p(r1[(n1 - i - n2) + j], r2[j])) break;
    return (j == parlay::size(r2));
  });
  return std::begin(r1) + n1 - idx - n2;
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto find_end(const R1& r1, const R2& r2) {
  return find_end(r1, r2,
    [](const auto& a, const auto& b) { return a == b; });
}

/* -------------------- Adjacent Finding -------------------- */

template <PARLAY_RANGE_TYPE R, typename BinaryPred>
auto adjacent_find(R&& r, BinaryPred p) {
  return std::begin(r) + internal::find_if_index(parlay::size(r) - 1,
    [&p, it = std::begin(r)](size_t i) {
      return p(it[i], it[i + 1]); });
}

template <PARLAY_RANGE_TYPE R>
auto adjacent_find(const R& r) {
  return std::begin(r) + internal::find_if_index(parlay::size(r) - 1,
    [it = std::begin(r)](size_t i) {
      return it[i] == it[i + 1]; });
}

/* ----------------------- Mismatch ----------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto mismatch(R1&& r1, R2&& r2) {
  auto d = internal::find_if_index(std::min(parlay::size(r1), parlay::size(r2)),
    [it1 = std::begin(r1), it2 = std::begin(r2)](size_t i) {
      return it1[i] != it2[i]; });
  return std::make_pair(std::begin(r1) + d, std::begin(r2) + d);
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto mismatch(R1&& r1, R2&& r2, BinaryPred p) {
  auto d = internal::find_if_index(std::min(parlay::size(r1), parlay::size(r2)),
    [&p, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i) {
      return !p(it1[i], it2[i]); });
  return std::make_pair(std::begin(r1) + d, std::begin(r2) + d);
}

/* ----------------------- Pattern search ----------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto search(R1&& r1, const R2& r2, BinaryPred pred) {
  return std::begin(r1) + internal::find_if_index(parlay::size(r1), [&](size_t i) {
    if (i + parlay::size(r2) > parlay::size(r1)) return false;
    size_t j;
    for (j = 0; j < parlay::size(r2); j++)
      if (!pred(r1[i + j], r2[j])) break;
    return (j == parlay::size(r2));
  });
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto search(R1&& r1, const R2& r2) {
  auto eq = [](const auto& a, const auto& b) { return a == b; };
  return search(r1, r2, eq);
}

/* ------------------------- Equal ------------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, class BinaryPred>
bool equal(const R1& r1, const R2& r2, BinaryPred p) {
  return parlay::size(r1) == parlay::size(r2) &&
    internal::find_if_index(parlay::size(r1),
      [&p, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i)
        { return !p(it1[i], it2[i]); }) == parlay::size(r1);
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
bool equal(const R1& r1, const R2& r2) {
  return equal(r1, r2,
    [](const auto& x, const auto& y) { return x == y; });
}

/* ---------------------- Lex compare ---------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, class Compare>
bool lexicographical_compare(const R1& r1, const R2& r2, Compare less) {
  size_t m = (std::min)(parlay::size(r1), parlay::size(r2));
  size_t i = internal::find_if_index(m,
    [&less, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i)
      { return less(it1[i], it2[i]) || less(it2[i], it1[i]); });
  return (i < m) ? (less(std::begin(r1)[i], std::begin(r2)[i]))
    : (parlay::size(r1) < parlay::size(r2));
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
inline bool lexicographical_compare(const R1& r1, const R2& r2) {
  return lexicographical_compare(r1, r2, std::less{});}

template <typename T>
inline bool operator<(const sequence<T> &a, 
		      const sequence<T> &b) {
  if (a.size() > 1000) 
    return lexicographical_compare(a, b);
  auto sa = a.begin();
  auto sb = b.begin();
  auto ea = sa + (std::min)(a.size(),b.size());
  while (sa < ea && *sa == *sb) {sa++; sb++;}
  return sa == ea ? (a.size() < b.size()) : *sa < *sb;
};


/* -------------------- Remove duplicates -------------------- */

template <PARLAY_RANGE_TYPE R, typename BinaryPredicate>
auto unique(const R& r, BinaryPredicate eq) {
  auto b = tabulate(
    parlay::size(r), [&eq, it = std::begin(r)](size_t i)
      { return (i == 0) || !eq(it[i], it[i - 1]); });
  return pack(r, b);
}

template<PARLAY_RANGE_TYPE R>
auto unique(const R& r) {
  return unique(r, std::equal_to<range_value_type_t<R>>());
}


/* -------------------- Min and max -------------------- */

// needs to return location, and take comparison
template <PARLAY_RANGE_TYPE R, typename Compare>
auto min_element(R&& r, Compare comp) {
  auto SS = delayed_seq<size_t>(parlay::size(r), [&](size_t i) { return i; });
  auto f = [&comp, it = std::begin(r)](size_t l, size_t r)
    { return (!comp(it[r], it[l]) ? l : r); };
  return std::begin(r) + internal::reduce(make_slice(SS), make_monoid(f, (size_t)parlay::size(r)));
}

template <PARLAY_RANGE_TYPE R>
auto min_element(R&& r) {
  return min_element(r, std::less<range_value_type_t<R>>());
}

template <PARLAY_RANGE_TYPE R, typename Compare>
auto max_element(R&& r, Compare comp) {
  return min_element(r, [&](const auto& a, const auto& b)
    { return comp(b, a); });
}

template <PARLAY_RANGE_TYPE R>
auto max_element(R&& r) {
  return max_element(r, std::less<range_value_type_t<R>>());
}

template <PARLAY_RANGE_TYPE R, typename Compare>
auto minmax_element(R&& r, Compare comp) {
  size_t n = parlay::size(r);
  auto SS = delayed_seq<std::pair<size_t, size_t>>(parlay::size(r),
    [&](size_t i) { return std::make_pair(i, i); });
  auto f = [&comp, it = std::begin(r)](const auto& l, const auto& r) {
    return (std::make_pair(!comp(it[r.first], it[l.first]) ? l.first : r.first,
              !comp(it[l.second], it[r.second]) ? l.second : r.second));
  };
  auto ds = internal::reduce(make_slice(SS), make_monoid(f, std::make_pair(n, n)));
  return std::make_pair(std::begin(r) + ds.first, std::begin(r) + ds.second);
}

template <PARLAY_RANGE_TYPE R>
auto minmax_element(R&& r) {
  return minmax_element(r, std::less<range_value_type_t<R>>());
}

/* -------------------- Permutations -------------------- */

template <PARLAY_RANGE_TYPE R>
auto reverse(const R& r) {
  auto n = parlay::size(r);
  return sequence<range_value_type_t<R>>::from_function(
    [n, it = std::begin(r)](size_t i) {
      return it[n - i - 1];
    });
}

template <PARLAY_RANGE_TYPE R>
auto reverse_inplace(R&& r) {
  auto n = parlay::size(r);
  parallel_for(0, n/2, [n, it = std::begin(r)] (size_t i) {
    std::swap(it[i], it[n - i - 1]);
  });
}

template <PARLAY_RANGE_TYPE R>
auto rotate(const R& r, size_t t) {
  size_t n = parlay::size(r);
  return sequence<range_value_type_t<R>>::from_function(parlay::size(r),
    [n, t, it = std::begin(r)](size_t i) {
      size_t j = (i < t) ? n - t + i : i - t;
      return it[j];
    });
}

/* -------------------- Is sorted? -------------------- */

template <PARLAY_RANGE_TYPE R, typename Compare>
bool is_sorted(const R& r, Compare comp) {
  auto B = delayed_seq<bool>(
    parlay::size(r) - 1, [&comp, it = std::begin(r)](size_t i)
      { return comp(it[i + 1], it[i]); });
  return (internal::reduce(make_slice(B), addm<size_t>()) != 0);
}

template <PARLAY_RANGE_TYPE R>
bool is_sorted(const R& r) {
  return is_sorted(r, std::less<range_value_type_t<R>>());
}

template <PARLAY_RANGE_TYPE R, typename Compare>
auto is_sorted_until(const R& r, Compare comp) {
  return std::begin(r) + internal::find_if_index(parlay::size(r) - 1,
    [&comp, it = std::begin(r)](size_t i) { return comp(it[i + 1], it[i]); }) + 1;
}

template <PARLAY_RANGE_TYPE R>
auto is_sorted_until(const R& r) {
  return is_sorted_until(r, std::less<range_value_type_t<R>>());
}

/* -------------------- Is partitioned? -------------------- */

template <PARLAY_RANGE_TYPE R, typename UnaryPred>
bool is_partitioned(const R& r, UnaryPred f) {
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

template <PARLAY_RANGE_TYPE R, typename UnaryPred>
auto remove_if(const R& r, UnaryPred pred) {
  auto flags = delayed_seq<bool>(parlay::size(r),
    [&pred, it = std::begin(r)](auto i) { return pred(it[i]); });
  return internal::pack(r, flags);
}

template<PARLAY_RANGE_TYPE R, typename T>
auto remove(const R& r, const T& v) {
  return remove_if(r, [&](const auto& x) { return x == v; });
}

/* -------------------- Iota -------------------- */

template <typename Index>
auto iota(Index n) {
  return delayed_seq<Index>(n, [&](size_t i) -> Index { return i; });
}

/* -------------------- Flatten -------------------- */

template <PARLAY_RANGE_TYPE R>
auto flatten(const R& r) {
  using T = range_value_type_t<range_value_type_t<R>>;
  auto offsets = tabulate(parlay::size(r),
    [it = std::begin(r)](size_t i) { return parlay::size(it[i]); });
  size_t len = internal::scan_inplace(make_slice(offsets), addm<size_t>());
  auto res = sequence<T>::uninitialized(len);
  parallel_for(0, parlay::size(r), [&, it = std::begin(r)](size_t i) {
      auto dit = std::begin(res)+offsets[i];
      auto sit = std::begin(it[i]);
      parallel_for(0, parlay::size(it[i]), [=] (size_t j) {
	  assign_uninitialized(*(dit+j), *(sit+j)); },
	1000);
  });
  return res;
}

// Guy :: should be merged with previous definition 
// this one does a move in the assign_uninitialized.
// If there was a way to clear a sequence withoud destructor 
//    then could relocate to the destination, saving a bunch of writes
template <typename T>
auto flatten(sequence<sequence<T>>&& r) {
  auto offsets = tabulate(parlay::size(r),
    [it = std::begin(r)](size_t i) { return parlay::size(it[i]); });
  size_t len = internal::scan_inplace(make_slice(offsets), addm<size_t>());
  auto res = sequence<T>::uninitialized(len);
  parallel_for(0, parlay::size(r), [&, it = std::begin(r)](size_t i) {
      auto dit = std::begin(res)+offsets[i];
      auto sit = std::begin(it[i]);
      parallel_for(0, parlay::size(it[i]), [=] (size_t j) {
					     //assign_uninitialized(*(dit+j), std::move(*(sit+j))); },
					     uninitialized_relocate((dit+j), (sit+j)); },
	1000);
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
template<PARLAY_RANGE_TYPE R, typename UnaryOp, typename UnaryPred = decltype(is_whitespace)>
auto map_tokens_old(R&& r, UnaryOp f, UnaryPred is_space = is_whitespace) {
  // internal::timer t("map tokens");
  using f_return_type = decltype(f(make_slice(r)));

  auto S = make_slice(r);
  size_t n = S.size();

  if (n == 0) {
    if constexpr (std::is_same_v<f_return_type, void>) {
      return;
    } else {
      return sequence<f_return_type>();
    }
  }

  // sequence<long> Locations = pack_index<long>(Flags);
  sequence<long> Locations = block_delayed::filter(iota<long>(n + 1), [&](size_t i) {
    return ((i == 0) ? !is_space(S[0]) : (i == n) ? !is_space(S[n - 1]) : is_space(S[i - 1]) != is_space(S[i]));
  });

  // If f does not return anything, just apply f
  // to each token and don't return anything
  if constexpr (std::is_same_v<f_return_type, void>) {
    parallel_for(0, Locations.size() / 2, [&](size_t i) { f(S.cut(Locations[2 * i], Locations[2 * i + 1])); });
  }
  // If f does return something, apply f to each token
  // and return a sequence of the resulting values
  else {
    auto x = tabulate(Locations.size() / 2, [&](size_t i) { return f(S.cut(Locations[2 * i], Locations[2 * i + 1])); });
    return x;
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
template<PARLAY_RANGE_TYPE R, typename UnaryOp, typename UnaryPred = decltype(is_whitespace)>
auto map_tokens(R&& r, UnaryOp f, UnaryPred is_space = is_whitespace) {
  using f_return_type = decltype(f(make_slice(r)));
  auto A = make_slice(r);
  using ipair = std::pair<long, long>;
  size_t n = A.size();

  if (n == 0) {
    if constexpr (std::is_same_v<f_return_type, void>)
      return;
    else
      return sequence<f_return_type>();
  }

  auto is_start = [&](size_t i) { return ((i == 0) || is_space(A[i - 1])) && (i != n) && !(is_space(A[i])); };
  auto is_end = [&](size_t i) { return ((i == n) || (is_space(A[i]))) && (i != 0) && !is_space(A[i - 1]); };
  // associative combining function
  // first = # of starts, second = index of last start
  auto g = [](ipair a, ipair b) { return (b.first == 0) ? a : ipair(a.first + b.first, b.second); };

  auto in = delayed_tabulate(n + 1, [&](size_t i) -> ipair { return is_start(i) ? ipair(1, i) : ipair(0, 0); });
  auto [offsets, sum] = block_delayed::scan(in, make_monoid(g, ipair(0, 0)));

  auto idxs = iota(n + 1);
  auto z = block_delayed::zip(offsets, idxs);

  auto start = r.begin();

  if constexpr (std::is_same_v<f_return_type, void>)
    block_delayed::apply(z, [&](auto x) {
      if (is_end(x.second)) f(make_slice(x.first.second + start, x.second + start));
    });
  else {
    auto result = sequence<f_return_type>::uninitialized(sum.first);
    block_delayed::apply(z, [&](auto x) {
      if (is_end(x.second))
        assign_uninitialized(result[x.first.first - 1], f(make_slice(start + x.first.second, start + x.second)));
    });
    return result;
  }
}

// Returns a sequence of tokens, each represented as a sequence of chars.
// The tokens are the longest contiguous subsequences of non space characters.
// where spaces are define by the unary predicate is_space. By default, is_space
// correponds to std::isspace, which is true for ' ', '\f', '\n', '\r'. '\t'. '\v'
template <PARLAY_RANGE_TYPE Range, typename UnaryPred = decltype(is_whitespace)>
sequence<parlay::chars> tokens(const Range& R, UnaryPred is_space = is_whitespace) {
  static_assert(std::is_convertible_v<range_value_type_t<Range>, char>);
  if (parlay::size(R) < 2000)
    return internal::map_tokens_old(R, [] (auto x) { return to_short_sequence(x); }, is_space);
  return map_tokens(R, [] (auto x) { return to_short_sequence(x); }, is_space);
}

// Partitions R into contiguous subsequences, by marking the last
// element of each subsequence with a true in flags.  There is an
// implied flag at the end.  Returns a nested sequence whose sum of
// lengths is equal to the original length.  If the last position is
// marked as true, then there will be an empty subsequence at the end.
// The length of the result is therefore always one more than the
// number of true flags.
template <PARLAY_RANGE_TYPE Range, PARLAY_RANGE_TYPE BoolRange>
auto split_at(const Range& R, const BoolRange& flags) {
  static_assert(std::is_convertible_v<range_value_type_t<BoolRange>, bool>);
  if constexpr (std::is_same<range_value_type_t<Range>, char>::value) {
    return map_split_at(R, flags, [](auto x) { return to_short_sequence(x); });
  }
  else {
    return map_split_at(R, flags, [](auto x) { return to_sequence(x); });
  }
}

// Like split_at, but applies the given function f to each of the
// contiguous subsequences of R delimited by positions i such that
// flags[i] is (or converts to) true.
template <PARLAY_RANGE_TYPE Range, PARLAY_RANGE_TYPE BoolRange, typename UnaryOp>
auto map_split_at(const Range& R, const BoolRange& flags, UnaryOp f) {
  static_assert(std::is_convertible_v<range_value_type_t<BoolRange>, bool>);
  
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

/* -------------------- Other Utilities -------------------- */

template <PARLAY_RANGE_TYPE R, class Compare>
auto remove_duplicates_ordered (const R& s, Compare less) {
  using T = range_value_type_t<R>;
  return unique(stable_sort(s, less), [&] (T a, T b) {
      return !less(a,b) && !less(b,a);});
}

// returns sequence with elementof same type as the first argument
template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto append (const R1& s1, const R2& s2) {
  using T = range_value_type_t<R1>;
  size_t n1 = s1.size();
  return tabulate(n1 + s2.size(), [&] (size_t i) -> T {
      return (i < n1) ? s1[i] : s2[i-n1];});
}

}  // namespace parlay

#include "internal/group_by.h"            // IWYU pragma: export

#endif  // PARLAY_PRIMITIVES_H_
