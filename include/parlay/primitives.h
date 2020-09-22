
#ifndef PARLAY_PRIMITIVES_H_
#define PARLAY_PRIMITIVES_H_

#include "internal/collect_reduce.h"
#include "internal/integer_sort.h"
#include "internal/sequence_ops.h"
#include "internal/sample_sort.h"

#include "monoid.h"
#include "range.h"
#include "sequence.h"
#include "slice.h"

namespace parlay {

/* -------------------- Map and Tabulate -------------------- */

// Return a sequence consisting of the elements
//   f(0), f(1), ... f(n)
using internal::tabulate;

// Return a sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto map(R&& r, UnaryOp&& f) {
  return tabulate(parlay::size(r), [&f, it = std::begin(r)](size_t i) {
    return f(it[i]); });
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
  return delayed_seq<typename std::remove_reference<
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

// Compute a histogram of the values of A, with m buckets.
template<PARLAY_RANGE_TYPE R, typename Integer_>
auto histogram(const R& A, Integer_ m) {
  return internal::histogram(make_slice(A), m);
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
    std::is_trivially_destructible_v<T>
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
  return internal::integer_sort(make_slice(in), [](auto x) { return x; }); 
}

template<PARLAY_RANGE_TYPE R, typename Key>
auto integer_sort(const R& in, Key&& key) {
  return internal::integer_sort(make_slice(in), std::forward<Key>(key));
}

template<PARLAY_RANGE_TYPE R>
void integer_sort_inplace(R&& in) {
  internal::integer_sort_inplace(make_slice(in), [](auto x) { return x; });
}

template<PARLAY_RANGE_TYPE R, typename Key>
void integer_sort_inplace(R&& in, Key&& key) {
  internal::integer_sort_inplace(make_slice(in), std::forward<Key>(key));
}

/* -------------------- Internal count and find -------------------- */

namespace internal {

template <typename IntegerPred>
size_t count_if_index(size_t n, IntegerPred p) {
  auto BS =
      delayed_seq<size_t>(n, [&](size_t i) -> size_t { return p(i); });
  size_t r = internal::reduce(make_slice(BS), addm<size_t>());
  return r;
}

template <typename IntegerPred>
size_t find_if_index(size_t n, IntegerPred p, size_t granularity = 1000) {
  size_t i;
  for (i = 0; i < std::min(granularity, n); i++)
    if (p(i)) return i;
  if (i == n) return n;
  size_t start = granularity;
  size_t block_size = 2 * granularity;
  std::atomic<size_t> result(n);
  while (start < n) {
    size_t end = std::min(n, start + block_size);
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
  return count_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <PARLAY_RANGE_TYPE R, class T>
size_t count(const R& r, T const &value) {
  return count_if_index(parlay::size(r),
    [&value, it = std::begin(r)](size_t i)
      { return it[i] == value; });
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
  return std::begin(r) + find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return p(it[i]); });
}

template <PARLAY_RANGE_TYPE R, typename T>
auto find(R&& r, T const &value) {
  return find_if(r, [&](const auto& x) { return x == value; });
}

template <PARLAY_RANGE_TYPE R, typename UnaryPredicate>
auto find_if_not(R&& r, UnaryPredicate p) {
  return std::begin(r) + find_if_index(parlay::size(r),
    [&p, it = std::begin(r)](size_t i) { return !p(it[i]); });
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPredicate>
auto find_first_of(R1&& r1, const R2& r2, BinaryPredicate p) {
  return std::begin(r1) + find_if_index(parlay::size(r1), [&] (size_t i) {
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
  size_t idx = find_if_index(parlay::size(r1) - parlay::size(r2) + 1, [&](size_t i) {
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
  return std::begin(r) + find_if_index(parlay::size(r) - 1,
    [&p, it = std::begin(r)](size_t i) {
      return p(it[i], it[i + 1]); });
}

template <PARLAY_RANGE_TYPE R>
size_t adjacent_find(const R& r) {
  return std::begin(r) + find_if_index(parlay::size(r) - 1,
    [it = std::begin(r)](size_t i) {
      return it[i] == it[i + 1]; });
}

/* ----------------------- Mismatch ----------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
auto mismatch(R1&& r1, R2&& r2) {
  auto d = find_if_index(std::min(parlay::size(r1), parlay::size(r2)),
    [it1 = std::begin(r1), it2 = std::begin(r2)](size_t i) {
      return it1[i] != it2[i]; });
  return std::make_pair(std::begin(r1) + d, std::begin(r2) + d);
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto mismatch(R1&& r1, R2&& r2, BinaryPred p) {
  auto d = find_if_index(std::min(parlay::size(r1), parlay::size(r2)),
    [&p, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i) {
      return !p(it1[i], it2[i]); });
  return std::make_pair(std::begin(r1) + d, std::begin(r2) + d);
}

/* ----------------------- Pattern search ----------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, typename BinaryPred>
auto search(R1&& r1, const R2& r2, BinaryPred pred) {
  return std::begin(r1) + find_if_index(parlay::size(r1), [&](size_t i) {
    if (i + parlay::size(r2) > parlay::size(r1)) return false;
    size_t j;
    for (j = 0; j < parlay::size(r2); j++)
      if (!pred(r1[i + j], r2[j])) break;
    return (j == parlay::size(r2));
  });
}

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2>
size_t search(R1&& r1, const R2& r2) {
  auto eq = [](const auto& a, const auto& b) { return a == b; };
  return std::begin(r1) + search(r1, r2, eq);
}

/* ------------------------- Equal ------------------------- */

template <PARLAY_RANGE_TYPE R1, PARLAY_RANGE_TYPE R2, class BinaryPred>
bool equal(const R1& r1, const R2& r2, BinaryPred p) {
  return parlay::size(r1) == parlay::size(r2) &&
    find_if_index(parlay::size(r1),
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
  size_t m = std::min(parlay::size(r1), parlay::size(r2));
  size_t i = find_if_index(m,
    [&less, it1 = std::begin(r1), it2 = std::begin(r2)](size_t i)
      { return less(it1[i], it2[i]) || less(it2[i], it1[i]); });
  return (i < m) ? (less(std::begin(r1)[i], std::begin(r2)[i]))
    : (parlay::size(r1) < parlay::size(r2));
}

/* -------------------- Remove duplicates -------------------- */

template <PARLAY_RANGE_TYPE R, typename BinaryPredicate>
auto unique(const R& r, BinaryPredicate eq) {
  auto b = delayed_seq<bool>(
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
  return std::begin(r) +
    internal::reduce(make_slice(SS), make_monoid(f, (size_t)parlay::size(r)));
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
  return std::begin(r) + find_if_index(parlay::size(r) - 1,
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
  auto d = find_if_index(n, [&f, it = std::begin(r)](size_t i) {
    return !f(it[i]);
  });
  if (d == n) return true;
  auto d2 = find_if_index(n - d - 1, [&f, it = std::begin(r) + d + 1](size_t i) {
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
  auto offsets = sequence<size_t>::from_function(parlay::size(r),
    [it = std::begin(r)](size_t i) { return parlay::size(it[i]); });
  size_t len = internal::scan_inplace(make_slice(offsets), addm<size_t>());
  auto res = sequence<T>::uninitialized(len);
  parallel_for(0, parlay::size(r), [&, it = std::begin(r)](size_t i) {
    parallel_for(0, parlay::size(it[i]),
      [&](size_t j) { assign_uninitialized(res[offsets[i] + j], it[i][j]); }
     );
  });
  return res;
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

#endif  // PARLAY_PRIMITIVES_H_
