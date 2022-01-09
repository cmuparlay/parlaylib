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

#include <cstddef>

#include <iterator>       // IWYU pragma: keep
#include <type_traits>
#include <utility>

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

/*  ----------------------------- Range traits --------------------------------
    Type traits that deduce useful types about a range.

    range_iterator_type  : the type of a range's iterator, i.e. the type of std::begin(r)
    range_sentinel_type  : the type of a range's sentinel, i.e., the type of std::end(r)
    range_value_type     : the value type of the contents of a range
    range_reference_type : the type obtained by dereferencing a range's iterator
*/

// Deduce the iterator type of the range type T
template<typename Range_>
struct range_iterator_type {
  using type = decltype(std::begin(std::declval<Range_&>()));
};

// Deduce the iterator type of the range type T
template<typename Range_>
using range_iterator_type_t = typename range_iterator_type<Range_>::type;

// Deduce the sentinel (end iterator) type of the range type T
template<typename Range_>
struct range_sentinel_type {
  using type = decltype(std::end(std::declval<Range_&>()));
};

// Deduce the sentinel (end iterator) type of the range type T
template<typename Range_>
using range_sentinel_type_t = typename range_sentinel_type<Range_>::type;

// Deduce the underlying value type of a range. This should correspond to a
// type that can be used to safely copy a value obtained by one of its iterators
template<typename Range_>
struct range_value_type {
  using type = typename std::iterator_traits<range_iterator_type_t<Range_>>::value_type;
};

// Deduce the underlying value type of a range. This should correspond to a
// type that can be used to safely copy a value obtained by one of its iterators
template<typename Range_>
using range_value_type_t = typename range_value_type<Range_>::type;

// Deduce the reference type of a range. This is the type obtained by dereferencing one of its iterators.
template<typename Range_>
struct range_reference_type {
  using type = typename std::iterator_traits<range_iterator_type_t<Range_>>::reference;
};

// Deduce the reference type of a range. This is the type obtained by dereferencing one of its iterators.
template<typename Range_>
using range_reference_type_t = typename range_reference_type<Range_>::type;


//  --------------------- Iterator concept traits -----------------------

// Provides the member value true if It_ is an iterator type
template<typename It_, typename = std::void_t<>>
struct is_iterator : public std::false_type {};

template<typename It_>
struct is_iterator<It_, std::void_t<
  typename std::iterator_traits<It_>::iterator_category
>> : public std::true_type {};

// True if It_ is an iterator type
template<typename It_>
inline constexpr bool is_iterator_v = is_iterator<It_>::value;

// Defines a member constant value true if the iterator type It_ is a contiguous iterator
template<typename It_>
struct is_contiguous_iterator : public std::is_pointer<It_> {};

// true if the iterator type It_ corresponds to a contiguous iterator type
template<typename It_>
inline constexpr bool is_contiguous_iterator_v = is_contiguous_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a random-access iterator
template<typename It_, typename = std::void_t<>>
struct is_random_access_iterator : public std::false_type {};

template<typename It_>
struct is_random_access_iterator<It_, std::void_t<
  std::enable_if_t<is_iterator_v<It_>>,
  std::enable_if_t<std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<It_>::iterator_category>>
>> : public std::true_type {};

// true if the iterator type It_ is at least a random-access iterator
template<typename It_>
inline constexpr bool is_random_access_iterator_v = is_random_access_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a bidirectional iterator
template<typename It_, typename = std::void_t<>>
struct is_bidirectional_iterator : public std::false_type {};

template<typename It_>
struct is_bidirectional_iterator<It_, std::void_t<
  std::enable_if_t<is_iterator_v<It_>>,
  std::enable_if_t<std::is_base_of_v<std::bidirectional_iterator_tag, typename std::iterator_traits<It_>::iterator_category>>
>> : public std::true_type {};

// true if the iterator type It is at least a bidirectional iterator
template<typename It_>
inline constexpr bool is_bidirectional_iterator_v = is_bidirectional_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a forward iterator
template<typename It_, typename = std::void_t<>>
struct is_forward_iterator : public std::false_type {};

template<typename It_>
struct is_forward_iterator<It_, std::void_t<
  std::enable_if_t<is_iterator_v<It_>>,
  std::enable_if_t<std::is_base_of_v<std::forward_iterator_tag, typename std::iterator_traits<It_>::iterator_category>>
>> : public std::true_type {};

// true if the iterator type It_ is at least a forward iterator
template<typename It_>
inline constexpr bool is_forward_iterator_v = is_forward_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least an input iterator
template<typename It_, typename = std::void_t<>>
struct is_input_iterator : public std::false_type {};

template<typename It_>
struct is_input_iterator<It_, std::void_t<
  std::enable_if_t<is_iterator_v<It_>>,
  std::enable_if_t<std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<It_>::iterator_category>>
>> : public std::true_type {};

// true if the iterator type It_ is at least an input iterator
template<typename It_>
inline constexpr bool is_input_iterator_v = is_input_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is an output iterator
template<typename It_, typename = std::void_t<>>
struct is_output_iterator : public std::false_type {};

template<typename It_>
struct is_output_iterator<It_, std::void_t<
  std::enable_if_t<is_iterator_v<It_>>,
  std::enable_if_t<std::is_base_of_v<std::output_iterator_tag, typename std::iterator_traits<It_>::iterator_category>>
>> : public std::true_type {};

// true if the iterator type It_ is an output iterator
template<typename It_>
inline constexpr bool is_output_iterator_v = is_output_iterator<It_>::value;

//  --------------------- Range concept traits -----------------------

// Provides the member value true if T is a range
template<typename, typename = std::void_t<>>
struct is_range : public std::false_type {};

template<typename Range_>
struct is_range<Range_, std::void_t<
  decltype(std::begin(std::declval<Range_&>())),
  decltype(std::end(std::declval<Range_&>())),
  std::enable_if_t<is_iterator_v<decltype(std::begin(std::declval<Range_&>()))>>,
  decltype(std::begin(std::declval<Range_&>()) == std::end(std::declval<Range_&>())),
  decltype(std::begin(std::declval<Range_&>()) != std::end(std::declval<Range_&>()))
>> : public std::true_type {};

// True if the given type is a range
template<typename Range_>
inline constexpr bool is_range_v = is_range<Range_>::value;

// Provides the member value true if T is a common range, i.e.,
// a range whose iterator and sentinel types are the same
template<typename, typename = std::void_t<>>
struct is_common_range : public std::false_type {};

template<typename Range_>
struct is_common_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<std::is_same_v<range_iterator_type_t<Range_>, range_sentinel_type_t<Range_>>>
>> : public std::true_type {};

// True if the given type is a common range, i.e., a range
// whose iterator and sentinel types are the same
template<typename Range_>
inline constexpr bool is_common_range_v = is_common_range<Range_>::value;

// Defines a member constant value true if the range Range_ is a contiguous range
template<typename, typename = std::void_t<>>
struct is_contiguous_range : public std::false_type {};

template<typename Range_>
struct is_contiguous_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_contiguous_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ corresponds to a contiguous range
template<typename Range_>
inline constexpr bool is_contiguous_range_v = is_contiguous_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a random-access range
template<typename, typename = std::void_t<>>
struct is_random_access_range : public std::false_type {};

template<typename Range_>
struct is_random_access_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_random_access_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ is a random-access range
template<typename Range_>
inline constexpr bool is_random_access_range_v = is_random_access_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a bidirectional range
template<typename, typename = std::void_t<>>
struct is_bidirectional_range : public std::false_type {};

template<typename Range_>
struct is_bidirectional_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_bidirectional_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ is a bidirectional range
template<typename Range_>
inline constexpr bool is_bidirectional_range_v = is_bidirectional_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a forward range
template<typename, typename = std::void_t<>>
struct is_forward_range : public std::false_type {};

template<typename Range_>
struct is_forward_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_forward_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ is a forward range
template<typename Range_>
inline constexpr bool is_forward_range_v = is_forward_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is an input range
template<typename, typename = std::void_t<>>
struct is_input_range : public std::false_type {};

template<typename Range_>
struct is_input_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_input_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ is an input range
template<typename Range_>
inline constexpr bool is_input_range_v = is_input_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is an output range
template<typename, typename = std::void_t<>>
struct is_output_range : public std::false_type {};

template<typename Range_>
struct is_output_range<Range_, std::void_t<
  std::enable_if_t<is_range_v<Range_>>,
  std::enable_if_t<is_output_iterator_v<range_iterator_type_t<Range_>>>
>> : public std::true_type {};

// true if the range type Range_ is an output range
template<typename Range_>
inline constexpr bool is_output_range_v = is_output_range<Range_>::value;

namespace internal {

// A type satisfies the block-iterable interface if it supports:
//
//  size() const -> size_t
//  get_num_blocks() const -> size_t
//  get_begin_block(size_t) -> iterator
//  get_end_block(size_t) -> iterator
//  get_begin_block(size_t) const -> const_iterator
//  get_end_block(size_t) const -> const_iterator
//
// where iterator and const_iterator is the common iterator type for the range when
// it is non-const or const respectively. iterator and const_iterator may be the same
// if that is semantically appropriate.
//
// Note that the const overloads are optional if the view can not be const.
//
template<typename, typename = std::void_t<>>
struct has_block_iterable_interface : public std::false_type {};

template<typename T>
struct has_block_iterable_interface<T, std::void_t<
  std::enable_if_t<std::is_convertible_v<decltype(std::declval<T&>().size()), size_t>>,
  std::enable_if_t<std::is_convertible_v<decltype(std::declval<T&>().get_num_blocks()), size_t>>,
  std::enable_if_t<std::is_same_v<decltype(std::declval<T&>().get_begin_block((size_t)0)), range_iterator_type_t<T&>>>,
  std::enable_if_t<std::is_same_v<decltype(std::declval<T&>().get_end_block((size_t)0)), range_iterator_type_t<T&>>>,
  std::enable_if_t<std::is_same_v<decltype(std::declval<T&>().get_begin_block((size_t)0)), decltype(std::declval<T&>().get_end_block((size_t)0))>>,
  decltype(std::declval<T&>().get_begin_block((size_t)0) == std::declval<T&>().get_end_block((size_t)0)),
  decltype(std::declval<T&>().get_begin_block((size_t)0) != std::declval<T&>().get_end_block((size_t)0))//,
  //std::enable_if_t<std::is_same_v<decltype(std::declval<const T&>().get_begin_block((size_t)0)), decltype(std::declval<const T&>().get_end_block((size_t)0))>>,
  //decltype(std::declval<const T&>().get_begin_block((size_t)0) == std::declval<const T&>().get_end_block((size_t)0)),
  //decltype(std::declval<const T&>().get_begin_block((size_t)0) != std::declval<const T&>().get_end_block((size_t)0))
>> : public std::true_type {};

template<typename T>
inline constexpr bool has_block_iterable_interface_v = has_block_iterable_interface<T>::value;

}  // namespace internal

// Provides a member constant value true if the range Range is block iterable.
// A range is block-iterable if it is a common range, and is either random access
// or it supports the block-iterable interface, meaning it provides the member
// functions size(), get_num_blocks(), get_begin_block(size_t), get_end_block(size_t)
template<typename, typename = std::void_t<>>
struct is_block_iterable_range : public std::false_type {};

template<typename Range_>
struct is_block_iterable_range<Range_, std::void_t<
  std::enable_if_t<is_common_range_v<Range_>>,
  std::enable_if_t<is_random_access_range_v<Range_> || internal::has_block_iterable_interface_v<Range_>>
>> : public std::true_type {};

// true if the range Range_ is block iterable
template<typename Range_>
inline constexpr bool is_block_iterable_range_v = is_block_iterable_range<Range_>::value;

/*  --------------------- Range operations -----------------------
    size(r) -> size_t : returns the size of a range
*/

// Return the size (number of elements) of the range r
template<typename Range_>
auto size(Range_&& r) {
  if constexpr (is_random_access_range_v<Range_>) {
    return std::end(r) - std::begin(r);
  }
  else {
    return r.size();
  }
}

}

#endif  // PARLAY_RANGE_H_
