// Parlay's sequence primitives are applicable to any types
// that satisfy the parlay::Range concept. This is a close
// approximation to the C++20 random_access_range and the
// sized_range concept. In the future, parlay::range will
// likely become an alias for those concepts.
//
// Broadly, a type satisfies parlay::range if it admits
// random access iterators via std::begin and std::end,
// and the size of the range can be determined by the
// expression std::end(t) - std::begin(t).
//
// If compiled with a concepts-supporting compiler, Parlay
// can check the inputs of all sequence operations against
// the Range concept. Otherwise, they reduce to regular non-
// constraining templates. The macros PARLAY_RANGE_TYPE and
// PARLAY_RANGE can be used for template arguments and for
// variable declarations respectively. For example:
//
// template<PARLAY_RANGE_TYPE R> auto f(R&& r);
//
// PARLAY_RANGE r = std::vector<int>{1,2,3};
//

#ifndef PARLAY_RANGE_H_
#define PARLAY_RANGE_H_

#include <iterator>       // IWYU pragma: keep
#include <type_traits>

#ifdef __cpp_concepts

namespace parlay {

// A type S is a size sentinal for a type I if given s and i of
// types S and I, [i, s] is a valid range and (s - i) is a valid
// expression that denotes the size of the range.
// When C++20 implementations of the standard library catch up,
// this can be replaced with the concept:
//   std::ranges::SizedSentinal
template<class S, class I>
concept SizedSentinel =
  requires(const I& i, const S& s) {
    i == s; s == i; i != s; s != i;
    s - i; requires std::is_same_v<decltype(s - i),
            typename std::iterator_traits<I>::difference_type>;
    i - s; requires std::is_same_v<decltype(i - s),
            typename std::iterator_traits<I>::difference_type>;
};

// An iterator type It is a random access iterator if it supports
// bidrectionally incremening and decrementing arbitrarily many
// positions in constant time.
// For now, we check the iterator_category tag to see whether it
// is marked as random access.
// When C++20 implementations of the standard library catch up,
// this can be replaced with the concept:
//   std::ranges::RandomAccessIterator.
template<typename It>
concept RandomAccessIterator = requires(It) {
  requires std::is_same<
    typename std::iterator_traits<It>::iterator_category,
    std::random_access_iterator_tag
  >::value;
};

// A Parlay range is any object that admits an iterator range
// via std::begin and std::end, such that the iterators are
// random access iterators. We also require the size of the
// range to be deducible via std::end(t) - std::begin(t).
// When C++20 implementations of the standard library catch up,
// this can be replaced with the concepts
//   std::ranges::random_access_range && std::ranges::sized_range
template<typename T>
concept Range = requires(T&& t) {
  std::begin(std::forward<T>(t));
  std::end(std::forward<T>(t));
  requires RandomAccessIterator<decltype(std::begin(t))>;
  requires SizedSentinel<decltype(std::end(t)), decltype(std::begin(t))>;
};

}

#define PARLAY_RANGE_TYPE parlay::Range
#define PARLAY_RANGE parlay::Range auto

#else

#define PARLAY_RANGE_TYPE typename
#define PARLAY_RANGE auto

#endif  // __cpp_concepts

namespace parlay {

// Return the size (numer of elements in) of the range r
// When C++20 implementations of the standard library catch up,
// this can be replaced with
//   std::ranges::size
template<PARLAY_RANGE_TYPE R>
auto size(const R& r) {
  return std::end(r) - std::begin(r);
}


// Deduce the underlying value type of a range
template<typename T>
struct range_value_type {
  using type = typename std::remove_cv<
               typename std::remove_reference<
                 decltype(
                   *std::begin(std::declval<T&>())
                 )
               >::type>::type;
};

template<typename T>
using range_value_type_t = typename range_value_type<T>::type;

}

#endif  // PARLAY_RANGE_H
