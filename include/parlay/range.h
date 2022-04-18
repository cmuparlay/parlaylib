// Type traits and concepts for range properties
//
// This file defines C++17 "concepts-lite" style traits
// for detecting properties of ranges, and for determining
// whether given types are a particular type of range.
//
// The concept traits do not exhaustively check that all
// requirements of the concept are satisfied, just most of
// the ones that are do-able enough in C++17 without
// going overboard. They should be more than thorough
// enough to use for SFINAE, and pretty good for sanity
// checking that an implementation of a container or range
// adapter has implemented the required features.
//
// Range Traits:
//   range_iterator_type  : the type of a range's iterator, i.e. the type of std::begin(r)
//   range_sentinel_type  : the type of a range's sentinel, i.e., the type of std::end(r)
//   range_value_type     : the value type of the contents of a range
//   range_reference_type : the type obtained by dereferencing a range's iterator
//
// Iterator Concepts:
//   is_iterator                : true if the given type is an iterator
//   is_sentinel_for            : true if the second type is a sentinel for the first (iterator) type
//   is_contiguous_iterator     : true if the given type is a contiguous iterator
//   is_random_access_iterator  : true if the given type is a random access iterator
//   is_bidirectional_iterator  : true if the given type is a bidirectional iterator
//   is_forward_iterator        : true if the given type is a forward iterator
//   is_input_iterator          : true if the given type is an input iterator
//   is_output_iterator         : true if the given type is an output iterator
//
// Range Concepts:
//   is_range                   : true if the given type is a range
//   is_common_range            : true if the given type is a common range (iterator and sentinel are the same type)
//   is_contiguous_range        : true if the given type is a contiguous range
//   is_random_access_range     : true if the given type is a random access range
//   is_bidirectional_range     : true if the given type is a bidirectional range
//   is_forward_range           : true if the given type is a forward range
//   is_input_ranger            : true if the given type is an input range
//   is_output_range            : true if the given type is an output range
//   is_block_iterable_range    : true if the given type is a block-iterable range
//
// Range Operations:
//   size : return the size of a range
//

#ifndef PARLAY_RANGE_H_
#define PARLAY_RANGE_H_

#include <cstddef>

#include <iterator>       // IWYU pragma: keep
#include <type_traits>
#include <utility>

#include "type_traits.h"

// Old macros that we used to use to conditionally
// insert type constraints if concepts were enabled.
// Here for backwards compatibility.
#define PARLAY_RANGE_TYPE typename
#define PARLAY_RANGE auto

namespace parlay {

/*  ----------------------------- Iterator traits --------------------------------
    Type traits that deduce useful types about an iterator.

    iterator_category        : the category tag type of the iterator
    iterator_value_type      : the value type of the iterator
    iterator_reference_type  : the type obtained by dereferencing an iterator
    iterator_difference_type : a type that can represent the difference between two iterators
*/

// Deduce the iterator category of the given iterator type
template<typename It_>
using iterator_category = type_identity<typename std::iterator_traits<It_>::iterator_category>;

template<typename It_>
using iterator_category_t = typename iterator_category<It_>::type;

// Deduce the value type of the given iterator type
template<typename It_>
using iterator_value_type = type_identity<typename std::iterator_traits<It_>::value_type>;

// Deduce the value type of the given iterator type
template<typename It_>
using iterator_value_type_t = typename iterator_value_type<It_>::type;

// Deduce the reference type of the given iterator type
template<typename It_>
using iterator_reference_type = type_identity<typename std::iterator_traits<It_>::reference>;

// Deduce the reference type of the given iterator type
template<typename It_>
using iterator_reference_type_t = typename iterator_reference_type<It_>::type;

// Deduce the difference type of the given iterator type
template<typename It_>
using iterator_difference_type = type_identity<typename std::iterator_traits<It_>::difference_type>;

// Deduce the difference type of the given iterator type
template<typename It_>
using iterator_difference_type_t = typename iterator_difference_type<It_>::type;

/*  ----------------------------- Range traits --------------------------------
    Type traits that deduce useful types about a range.

    range_iterator_type   : the type of a range's iterator, i.e. the type of std::begin(r)
    range_sentinel_type   : the type of a range's sentinel, i.e., the type of std::end(r)
    range_value_type      : the value type of the contents of a range
    range_reference_type  : the type obtained by dereferencing a range's iterator
    range_difference_type : a type that can represent the difference between two iterators
*/

// Deduce the iterator type of the range type T
template<typename Range_>
using range_iterator_type = type_identity<decltype(std::begin(std::declval<Range_&>()))>;

// Deduce the iterator type of the range type T
template<typename Range_>
using range_iterator_type_t = typename range_iterator_type<Range_>::type;

// Deduce the sentinel (end iterator) type of the range type T
template<typename Range_>
using range_sentinel_type = type_identity<decltype(std::end(std::declval<Range_&>()))>;

// Deduce the sentinel (end iterator) type of the range type T
template<typename Range_>
using range_sentinel_type_t = typename range_sentinel_type<Range_>::type;

// Deduce the underlying value type of a range. This should correspond to a
// type that can be used to safely copy a value obtained by one of its iterators
template<typename Range_>
using range_value_type = type_identity<iterator_value_type_t<range_iterator_type_t<Range_>>>;

// Deduce the underlying value type of a range. This should correspond to a
// type that can be used to safely copy a value obtained by one of its iterators
template<typename Range_>
using range_value_type_t = typename range_value_type<Range_>::type;

// Deduce the reference type of a range. This is the type obtained by dereferencing one of its iterators.
template<typename Range_>
using range_reference_type = type_identity<iterator_reference_type_t<range_iterator_type_t<Range_>>>;

// Deduce the reference type of a range. This is the type obtained by dereferencing one of its iterators.
template<typename Range_>
using range_reference_type_t = typename range_reference_type<Range_>::type;

// Deduce the difference type of a range. This is a type that can be used to
// represent a difference between two iterators
template<typename Range_>
using range_difference_type = type_identity<iterator_difference_type_t<range_iterator_type_t<Range_>>>;

// Deduce the difference type of a range. This is a type that can be used to
//// represent a difference between two iterators
template<typename Range_>
using range_difference_type_t = typename range_difference_type<Range_>::type;

//  --------------------- Iterator concept traits -----------------------

// Provides the member value true if It_ is an iterator type
template<typename It_, typename = std::void_t<>>
struct is_iterator : public std::false_type {};

template<typename It_>
struct is_iterator<It_, std::void_t<
  iterator_category<It_>,
  decltype( ++std::declval<It_&>() ),
  decltype( *std::declval<It_&>() ),
  std::enable_if_t< std::is_swappable_v<It_&> >,
  std::enable_if_t< std::is_same_v<decltype(++std::declval<It_&>()), It_&> >,
  std::enable_if_t< std::is_copy_constructible_v<It_> >,
  std::enable_if_t< std::is_copy_assignable_v<It_> >,
  std::enable_if_t< std::is_destructible_v<It_> >
>> : public std::true_type {};

// True if It_ is an iterator type
template<typename It_>
inline constexpr bool is_iterator_v = is_iterator<It_>::value;

// Provides the member value true if Sentinel_ is a sentinel type
// for the iterator type It_
template<typename It_, typename Sentinel_, typename = std::void_t<>>
struct is_sentinel_for : public std::false_type {};

template<typename It_, typename Sentinel_>
struct is_sentinel_for<It_, Sentinel_, std::void_t<
  std::enable_if_t< is_iterator_v<It_> >,
  std::enable_if_t< is_equality_comparable_v<It_, Sentinel_> >,
  std::enable_if_t< std::is_default_constructible_v<Sentinel_> >,
  std::enable_if_t< std::is_copy_constructible_v<Sentinel_> >,
  std::enable_if_t< std::is_copy_assignable_v<Sentinel_> >
>> : public std::true_type {};

// True if Sentinel_ is a sentinel type for the iterator It_
template<typename It_, typename Sentinel_>
inline constexpr bool is_sentinel_for_v = is_sentinel_for<It_, Sentinel_>::value;

// Provides the member value true if Sentinel_ is a sized sentinel type
// for the iterator type It_
template<typename It_, typename Sentinel_, typename = std::void_t<>>
struct is_sized_sentinel_for : public std::false_type {};

template<typename It_, typename Sentinel_>
struct is_sized_sentinel_for<It_, Sentinel_, std::void_t<
  std::enable_if_t< is_sentinel_for_v<It_, Sentinel_> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_>() - std::declval<Sentinel_>()), iterator_difference_type_t<It_>> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<Sentinel_>() - std::declval<It_>()), iterator_difference_type_t<It_>> >
>> : std::true_type {};

// True if Sentinel_ is a sized sentinel type for the iterator It_
template<typename It_, typename Sentinel_>
inline constexpr bool is_sized_sentinel_for_v = is_sized_sentinel_for<It_, Sentinel_>::value;

// Defines a member constant value true if the iterator It_ is at least an input iterator
template<typename It_, typename = std::void_t<>>
struct is_input_iterator : public std::false_type {};

template<typename It_>
struct is_input_iterator<It_, std::void_t<
  decltype( std::declval<It_&>()++ ),
  std::enable_if_t< is_iterator_v<It_> >,
  std::enable_if_t< is_equality_comparable_v<It_&, It_&> >,
  std::enable_if_t< std::is_base_of_v<std::input_iterator_tag, iterator_category_t<It_>> >,
  std::enable_if_t< std::is_same_v<decltype(*std::declval<It_&>()), iterator_reference_type_t<It_>> >
>> : public std::true_type {};

// true if the iterator type It_ is at least an input iterator
template<typename It_>
inline constexpr bool is_input_iterator_v = is_input_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is an output iterator
template<typename It_, typename = std::void_t<>>
struct is_output_iterator : public std::false_type {};

template<typename It_>
struct is_output_iterator<It_, std::void_t<
  std::enable_if_t< is_iterator_v<It_> >,
  std::enable_if_t< std::is_base_of_v<std::output_iterator_tag, iterator_category_t<It_>> >
>> : public std::true_type {};

// true if the iterator type It_ is an output iterator
template<typename It_>
inline constexpr bool is_output_iterator_v = is_output_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a forward iterator
template<typename It_, typename = std::void_t<>>
struct is_forward_iterator : public std::false_type {};

template<typename It_>
struct is_forward_iterator<It_, std::void_t<
  std::enable_if_t< is_input_iterator_v<It_> >,
  std::enable_if_t< std::is_base_of_v<std::forward_iterator_tag, iterator_category_t<It_>> >,
  std::enable_if_t< std::is_default_constructible_v<It_> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_&>()++), const std::remove_reference_t<It_>&> >,
  std::enable_if_t< std::is_same_v<decltype(*(std::declval<It_&>()++)), iterator_reference_type_t<It_>> >
>> : public std::true_type {};

// true if the iterator type It_ is at least a forward iterator
template<typename It_>
inline constexpr bool is_forward_iterator_v = is_forward_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a bidirectional iterator
template<typename It_, typename = std::void_t<>>
struct is_bidirectional_iterator : public std::false_type {};

template<typename It_>
struct is_bidirectional_iterator<It_, std::void_t<
  decltype( --std::declval<It_&>() ),
  decltype( std::declval<It_&>()-- ),
  std::enable_if_t< is_forward_iterator_v<It_> >,
  std::enable_if_t< std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category_t<It_>> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_&>()--), const std::remove_reference_t<It_>&> >,
  std::enable_if_t< std::is_same_v<decltype(*(std::declval<It_&>()--)), iterator_reference_type_t<It_>> >
>> : public std::true_type {};

// true if the iterator type It is at least a bidirectional iterator
template<typename It_>
inline constexpr bool is_bidirectional_iterator_v = is_bidirectional_iterator<It_>::value;

// Defines a member constant value true if the iterator It_ is at least a random-access iterator
template<typename It_, typename = std::void_t<>>
struct is_random_access_iterator : public std::false_type {};

template<typename It_>
struct is_random_access_iterator<It_, std::void_t<
  std::enable_if_t< is_bidirectional_iterator_v<It_> >,
  std::enable_if_t< std::is_base_of_v<std::random_access_iterator_tag, iterator_category_t<It_>> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_&>() += std::declval<iterator_difference_type_t<It_>>()), It_&> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_>() + std::declval<iterator_difference_type_t<It_>>()), It_> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_&>() -= std::declval<iterator_difference_type_t<It_>>()), It_&> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_>() - std::declval<iterator_difference_type_t<It_>>()), It_> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_>() - std::declval<It_>()), iterator_difference_type_t<It_>> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<It_>()[std::declval<iterator_difference_type_t<It_>>()]), iterator_reference_type_t<It_>> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_>() < std::declval<It_>()), bool> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_>() > std::declval<It_>()), bool> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_>() >= std::declval<It_>()), bool> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<It_>() <= std::declval<It_>()), bool> >
>> : public std::true_type {};

// true if the iterator type It_ is at least a random-access iterator
template<typename It_>
inline constexpr bool is_random_access_iterator_v = is_random_access_iterator<It_>::value;

// Defines a member constant value true if the iterator type It_ is a contiguous iterator
template<typename It_>
using is_contiguous_iterator = std::is_pointer<It_>;

// true if the iterator type It_ corresponds to a contiguous iterator type
template<typename It_>
inline constexpr bool is_contiguous_iterator_v = is_contiguous_iterator<It_>::value;


//  --------------------- Range concept traits -----------------------

// Provides the member value true if Range_ is a range
template<typename, typename = std::void_t<>>
struct is_range : public std::false_type {};

template<typename Range_>
struct is_range<Range_, std::void_t<
  decltype( std::begin(std::declval<Range_&>()) ),
  decltype( std::end(std::declval<Range_&>()) ),
  std::enable_if_t< is_sentinel_for_v<range_iterator_type_t<Range_>, range_sentinel_type_t<Range_>> >
>> : public std::true_type {};

// True if the given type Range_ is a range
template<typename Range_>
inline constexpr bool is_range_v = is_range<Range_>::value;

// Provides the member value true if Range_ is a common range,
// i.e., a range whose iterator and sentinel types are the same
template<typename, typename = std::void_t<>>
struct is_common_range : public std::false_type {};

template<typename Range_>
struct is_common_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< std::is_same_v<range_iterator_type_t<Range_>, range_sentinel_type_t<Range_>> >
>> : public std::true_type {};

// True if the given type is a common range, i.e., a range
// whose iterator and sentinel types are the same
template<typename Range_>
inline constexpr bool is_common_range_v = is_common_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is an input range
template<typename, typename = std::void_t<>>
struct is_input_range : public std::false_type {};

template<typename Range_>
struct is_input_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_input_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ is an input range
template<typename Range_>
inline constexpr bool is_input_range_v = is_input_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is an output range
template<typename, typename = std::void_t<>>
struct is_output_range : public std::false_type {};

template<typename Range_>
struct is_output_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_output_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ is an output range
template<typename Range_>
inline constexpr bool is_output_range_v = is_output_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a forward range
template<typename, typename = std::void_t<>>
struct is_forward_range : public std::false_type {};

template<typename Range_>
struct is_forward_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_forward_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ is a forward range
template<typename Range_>
inline constexpr bool is_forward_range_v = is_forward_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a bidirectional range
template<typename, typename = std::void_t<>>
struct is_bidirectional_range : public std::false_type {};

template<typename Range_>
struct is_bidirectional_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_bidirectional_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ is a bidirectional range
template<typename Range_>
inline constexpr bool is_bidirectional_range_v = is_bidirectional_range<Range_>::value;

// Defines a member constant value true if the range type Range_ is a random-access range
template<typename, typename = std::void_t<>>
struct is_random_access_range : public std::false_type {};

template<typename Range_>
struct is_random_access_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_random_access_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ is a random-access range
template<typename Range_>
inline constexpr bool is_random_access_range_v = is_random_access_range<Range_>::value;

// Defines a member constant value true if the range Range_ is a contiguous range
template<typename, typename = std::void_t<>>
struct is_contiguous_range : public std::false_type {};

template<typename Range_>
struct is_contiguous_range<Range_, std::void_t<
  std::enable_if_t< is_range_v<Range_> >,
  std::enable_if_t< is_contiguous_iterator_v<range_iterator_type_t<Range_>> >
>> : public std::true_type {};

// true if the range type Range_ corresponds to a contiguous range
template<typename Range_>
inline constexpr bool is_contiguous_range_v = is_contiguous_range<Range_>::value;

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
  decltype( std::declval<T&>().get_begin_block((size_t)0) ),
  decltype( std::declval<T&>().get_end_block((size_t)0) ),
  decltype( std::declval<T&>().size() ),
  decltype( std::declval<T&>().get_num_blocks() ),
  std::enable_if_t< std::is_same_v<decltype(std::declval<T&>().get_begin_block((size_t)0)), range_iterator_type_t<T&>> >,
  std::enable_if_t< std::is_same_v<decltype(std::declval<T&>().get_end_block((size_t)0)), range_iterator_type_t<T&>> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<T&>().size()), size_t> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<T&>().get_num_blocks()), size_t> >
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
  std::enable_if_t< is_common_range_v<Range_> >,
  std::enable_if_t< is_random_access_range_v<Range_> || internal::has_block_iterable_interface_v<Range_> >
>> : public std::true_type {};

// true if the range Range_ is block iterable
template<typename Range_>
inline constexpr bool is_block_iterable_range_v = is_block_iterable_range<Range_>::value;

/*  --------------------- Range size -----------------------
    size(r) -> size_t : returns the size of a range
*/

// Defines the member value true if the given type is an array of known bound
template<typename T>
struct is_bounded_array: std::false_type {};

template<typename T, std::size_t N>
struct is_bounded_array<T[N]> : std::true_type {};

// true if the given type is an array of known bound
template<typename T>
inline constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

namespace internal {

// Defines the member value true if the given type has a member function size()
// which takes no arguments
template<typename, typename = std::void_t<>>
struct has_size_member : public std::false_type {};

template<typename Range_>
struct has_size_member<Range_, std::void_t<
  decltype( std::declval<Range_>().size() )
>> : std::true_type {};

// true if the given type has a member function size() which takes no arguments
template<typename Range_>
inline constexpr bool has_size_member_v = has_size_member<Range_>::value;

namespace nonmember_size_impl {

// Deleting the size function in this scope forces overload
// resolution to look for ADL matches for the size function

template<typename T>
void size(T&) = delete;

template<typename T>
void size(const T&) = delete;

// Defines the member value true if the given type has a non-
// member function size(r) that can be found via ADL
template<typename, typename = std::void_t<>>
struct has_nonmember_size : public std::false_type {};

template<typename Range_>
struct has_nonmember_size<Range_, std::void_t<
 decltype( size(std::declval<Range_>()) )
>> : std::true_type {};

// true if the given type has a non-member function size(r) that can be found via ADL
template<typename Range_>
inline constexpr bool has_nonmember_size_v = has_nonmember_size<Range_>::value;

// Invokes the non-member size function for the given range
template<typename Range_>
auto invoke_size(Range_&& r) {
  static_assert(has_nonmember_size_v<Range_>);
  return size(std::forward<Range_>(r));
}

}  // namespace nonmember_size_impl

// The implementation of parlay::size is hidden inside a namespace
// and pulled in with a using directive to hide it from ADL
namespace size_impl {

// Return the size (number of elements) of the range r. This is:
//
// * std::extent_v<Range_>          if Range_ is a bounded array type
// * FORWARD(r).size()              if Range_ is a type with a .size() member
// * size(FORWARD(r))               if a non-member size function is found by ADL
// * std::end(r) - std::begin(r)    if begin(r) and end(r) model a sized-sentinel pair
//
// In the future, this could be replaced by C++20 std::ranges::size
template<typename Range_>
auto size(Range_&& r) {
  static_assert(is_range_v<Range_>);
  static_assert(is_bounded_array_v<Range_> ||
                has_size_member_v<Range_> ||
                nonmember_size_impl::has_nonmember_size_v<Range_> ||
                is_sized_sentinel_for_v<range_iterator_type_t<Range_>, range_sentinel_type_t<Range_>>);

  if constexpr (is_bounded_array_v<Range_>) {
    return std::extent_v<Range_>;
  }
  else if constexpr (has_size_member_v<Range_>) {
    return std::forward<Range_>(r).size();
  }
  else if constexpr (nonmember_size_impl::has_nonmember_size_v<Range_>) {
    return nonmember_size_impl::invoke_size(std::forward<Range_>(r));
  }
  else {
    return std::make_unsigned_t<range_difference_type_t<Range_>>(std::end(r) - std::begin(r));
  }
}

}  // size_impl

} // namespace internal

using internal::size_impl::size;

// A functional that takes a range r and returns its size as given by parlay::size(FORWARD(r))
struct size_of {
  template<typename Range_>
  decltype(auto) operator()(Range_&& r) const { return parlay::size(std::forward<Range_>(r)); }
};

}  // namespace parlay

#endif  // PARLAY_RANGE_H_
