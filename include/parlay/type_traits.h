// Useful type traits used mostly internally by Parlay
//
// Many inspired by this video:
//  - https://www.youtube.com/watch?v=MWBfmmg8-Yo
//
// Notable inclusions:
//  - is_trivially_relocatable
//

#ifndef PARLAY_TYPE_TRAITS_H_
#define PARLAY_TYPE_TRAITS_H_

#include <cstddef>

#include <array>
#include <deque>
#include <functional>
#include <forward_list>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <type_traits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>       // IWYU pragma: keep
#include <variant>

// IWYU pragma: no_include <iterator>

namespace parlay {

// Provides the member type T
template<typename T>
struct type_identity {
  using type = T;
};

// Equal to the type T, i.e., the identity transformation
template<typename T>
using type_identity_t = typename type_identity<T>::type;

// Provides the member type equivalent to T with its cv-ref qualifiers removed
template<typename T>
struct remove_cvref : type_identity<std::remove_cv_t<std::remove_reference_t<T>>> { };

// Equal to T with its cv-ref qualifiers removed
template<typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Given a pointer-to-member (object or function), returns
// the type of the class in which the member lives
template<typename T>
struct member_pointer_class;

template<typename T, typename U>
struct member_pointer_class<T U::*> : public type_identity<U> {};

template<typename T>
using member_pointer_class_t = typename member_pointer_class<T>::type;

// Provides the member type std::add_const_t<T> if Const is
// true, otherwise provides the member type T
template<bool Const, typename T>
using maybe_const = std::conditional<Const, std::add_const_t<T>, T>;

// Adds const to the given type if Const is true
template<bool Const, typename T>
using maybe_const_t = typename maybe_const<Const, T>::type;

// Provides the member type std::decay_t<T> if Decay is
// true, otherwise provides the member type T
template<bool Decay, typename T>
using maybe_decay = std::conditional<Decay, std::decay_t<T>, T>;

// Decays the given type if Decay is true
template<bool Decay, typename T>
using maybe_decay_t = typename maybe_decay<Decay, T>::type;

// Provides the member value true if the given type is an instance of std::optional
template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

// true if the given type is an instance of std::optional
template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template<typename T, typename U = T>
using is_less_than_comparable = std::conjunction<
                                  std::is_invocable_r<bool, std::less<>, T, U>,
                                  std::is_invocable_r<bool, std::less<>, U, T>
                                >;

template<typename T, typename U = T>
inline constexpr bool is_less_than_comparable_v = is_less_than_comparable<T, U>::value;

template<typename T, typename U = T>
using is_equality_comparable = std::conjunction<
                                 std::is_invocable_r<bool, std::equal_to<>, T, U>,
                                 std::is_invocable_r<bool, std::equal_to<>, U, T>,
                                 std::is_invocable_r<bool, std::not_equal_to<>, T, U>,
                                 std::is_invocable_r<bool, std::not_equal_to<>, U, T>
                               >;

template<typename T, typename U = T>
inline constexpr bool is_equality_comparable_v = is_equality_comparable<T, U>::value;

// Defines a member value true if the given type BinaryOperator_ can be invoked on types
// T1&& and T2 to yield a result of a type that is convertible to T1.
//
// This requirement corresponds to the needs of a left fold over the operator BinaryOperator_
// with an identity and result type of T1, where the intermediate elements being reduced over
// are potentially of type T2.
template<typename BinaryOperator_, typename T1, typename T2, typename = void, typename = void>
struct is_binary_operator_for : public std::false_type {};

template<typename BinaryOperator_, typename T1, typename T2>
struct is_binary_operator_for<BinaryOperator_, T1, T2, std::void_t<
  std::enable_if_t< std::is_move_constructible_v<T1> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, T1&&, T1&&> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, T1&&, T2> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, T2, T2> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, T2, T1&&> >
>, std::enable_if_t<!std::is_member_function_pointer_v<BinaryOperator_>>> : public std::true_type{};

// Handle the case where BinaryOperator_ is a member function pointer
template<typename BinaryOperator_, typename T1, typename T2>
struct is_binary_operator_for<BinaryOperator_, T1, T2, std::void_t<
  std::enable_if_t< std::is_move_constructible_v<T1> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, const member_pointer_class_t<BinaryOperator_>&, T1&&, T1&&> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, const member_pointer_class_t<BinaryOperator_>&, T1&&, T2> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, const member_pointer_class_t<BinaryOperator_>&, T2, T2> >,
  std::enable_if_t< std::is_invocable_r_v<T1, BinaryOperator_, const member_pointer_class_t<BinaryOperator_>&, T2, T1&&> >
>, std::enable_if_t<std::is_member_function_pointer_v<BinaryOperator_>>> : public std::true_type{};

// True if the given type BinaryOperator_ can be invoked on types T1&& and T2 to yield a result
// of a type that is convertible to T1. T2 defaults to T1&& if not specified.
//
// This requirement corresponds to the needs of a left fold over the operator BinaryOperator_
// with an identity and result type of T1, where the intermediate elements being reduced over
// are potentially of type T2.
template<typename BinaryOperator_, typename T1, typename T2 = T1&&>
inline constexpr bool is_binary_operator_for_v = is_binary_operator_for<BinaryOperator_, T1, T2>::value;

// Defines the member value true if T is a pair or a tuple of length two
template<typename T, typename = void>
struct is_pair : public std::false_type {};

template<typename T>
struct is_pair<T, std::void_t<
  decltype( std::get<0>(std::declval<T>()) ),
  decltype( std::get<1>(std::declval<T>()) ),
  std::enable_if_t< 2 == std::tuple_size_v<std::decay_t<T>> >
>> : public std::true_type {};

// True if T is a pair or a tuple of length two
template<typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

/*  --------------------- Priority tags. -------------------------
    Priority tags are an easy way to force template resolution to
    pick the "best" option in the presence of multiple valid
    choices. It works because of the facts that priority_tag<K>
    is a subtype of priority_tag<K-1>, and template resolution
    will always pick the most specialised option when faced with
    a choice, so it will prefer priority_tag<K> over
    priority_tag<K-1>
*/

template<size_t K>
struct priority_tag : priority_tag<K-1> {};

template<>
struct priority_tag<0> {};


/*  ----------------- Trivial allocators. ---------------------
    Allocator-aware containers and algorithms need to know whether
    they can construct/destruct objects directly inside memory given
    to them by an allocator, or whether the allocator has custom
    behaviour. Since some optimizations require us to circumvent
    custom allocator behaviour, we need to detect when an allocator
    does not do this.

    Specifically, an allocator-aware algorithm must construct objects
    inside memory returned by an allocator by writing

    std::allocator_traits<allocator_type>::construct(allocator, p, args);

    if the allocator type defines a method .construct, then this results
    in forwarding the construction to that method. Otherwise, this just
    results in a call to

    new (p) T(std::forward<Args>(args)...)

    If we wish to circumvent calling the constructor, for example,
    for a trivially relocatable type in which we would prefer to
    copy directly via memcpy, we must ensure that the allocator
    does not have a custom .construct method. Otherwise, we can
    not optimize, and must continue to use the allocator's own
    construct method.

    The same discussion is true for destruction as well.

    See https://www.youtube.com/watch?v=MWBfmmg8-Yo for more info.
*/

namespace internal {

// Detect the existence of the .destroy method of the type Alloc
template<typename Alloc, typename T>
auto trivial_allocator(Alloc& a, T* p, priority_tag<2>)
  -> decltype(void(a.destroy(p)), std::false_type());

// Detect the existence of the .construct method of the type Alloc
template<typename Alloc, typename T>
auto trivial_allocator(Alloc& a, T* p, priority_tag<1>)
  -> decltype(void(a.construct(p, std::declval<T&&>())), std::false_type());

// By default, if no .construct or .destroy methods are found, assume
// that the allocator is trivial
template<typename Alloc, typename T>
auto trivial_allocator(Alloc& a, T* p, priority_tag<0>)
  -> std::true_type;

}  // namespace internal

template<typename Alloc, typename T>
struct is_trivial_allocator
    : decltype(internal::trivial_allocator<Alloc, T>(std::declval<Alloc&>(), nullptr, priority_tag<2>())) {};

template<typename Alloc, typename T>
inline constexpr bool is_trivial_allocator_v = is_trivial_allocator<Alloc, T>::value;

// Manually specialize std::allocator since it is trivial, but
// some (maybe all?) implementations still provide a .construct
// and .destroy method anyway.
template<typename T>
struct is_trivial_allocator<std::allocator<T>, T> : std::true_type {};


/*  ----------------- Trivially relocatable. ---------------------

    A type T is called trivially relocatable if, given a pointer p
    to an object of type T, and a pointer q to uninitialized memory
    large enough for an object of type T, then

      ::new (voidify(*q)) T(std::move(*p));
      std::destroy_at(p);

    is equivalent to

      std::memcpy(p, q, sizeof(T));

    Any type that is trivially copyable is trivially relocatable, but many types
    that are not trivially copyable are trivially relocatable. User-defined types
    that are not obviously trivially relocatable can be annotated as such (with
    care). This is used to optimize parlay::sequence so that it can reallocate
    much faster, and is also used by sorting algorithms for faster swapping of
    ranges of elements and individual elements.

    See proposal P1144 for copious details:
    https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1144r9.html
*/

// Standard language support for trivially relocatable (currently P1144)
#if defined(__cpp_lib_trivially_relocatable)

template<typename T>
using is_trivially_relocatable = std::is_trivially_relocatable<T>;

// Clang's builtin with some (very limited) support for trivial relocatability
#elif defined(__has_builtin)
#if __has_builtin(__is_trivially_relocatable)

template<typename T>
struct is_trivially_relocatable : std::bool_constant<__is_trivially_relocatable(T)> { };

#define PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE

#else

template<typename T>
struct is_trivially_relocatable : std::is_trivially_copyable<T> { };

#define PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE

#endif
#else

template<typename T>
struct is_trivially_relocatable : std::is_trivially_copyable<T> { };

#define PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE

#endif

template<typename T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

#if defined(__cpp_impl_trivially_relocatable)
#define PARLAY_TRIVIALLY_RELOCATABLE [[trivially_relocatable]]
#else
#define PARLAY_TRIVIALLY_RELOCATABLE
#endif


#if defined(PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE)

#define PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(...)                              \
  struct is_trivially_relocatable<__VA_ARGS__> : std::true_type {};

#define PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF(cond, ...)                     \
  struct is_trivially_relocatable<__VA_ARGS__> : std::bool_constant<cond> {};


// We list all the types that ought to be trivially relocatable under any
// sensible implementation. It is possible that some of these are wrong.

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::allocator<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::shared_ptr<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::weak_ptr<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<T>), std::optional<T>);

template<typename T, typename Deleter>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<Deleter> &&
    is_trivially_relocatable_v<typename std::unique_ptr<T, Deleter>::pointer>), std::unique_ptr<T, Deleter>);

template<typename T1, typename T2>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<T1> &&
                                        is_trivially_relocatable_v<T2>), std::pair<T1, T2>);

template<typename T, std::size_t N>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<T>), std::array<T, N>);

template <typename... T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<T> && ...), std::tuple<T...>);

template <typename... T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE_IF((is_trivially_relocatable_v<T> && ...), std::variant<T...>);


// Container implementations can differ widely by vendor, so we don't want to specialize these
// with a broad brush.  Instead, only specialize them for stdlibs that we are confident about,
// or, more specifically, stdlibs that Arthur is confident about :D
//
// https://quuxplusone.github.io/blog/2019/02/20/p1144-what-types-are-relocatable/

// Specializations for libc++.
#if defined(_LIBCPP_VERSION)

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::deque<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::forward_list<T>);

// std::vector and std::string are not trivially relocatable in libc++ debug mode
#if !defined(_LIBCPP_DEBUG_MODE)

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::vector<T>);

template<>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::string);

#endif

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::unordered_set<T>);

template<typename K, typename V>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::unordered_map<K, V>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::unordered_multiset<T>);

template<typename K, typename V>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::unordered_multimap<K, V>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::stack<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::queue<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::priority_queue<T>);


// Specializations for GCC.
#elif defined(__GLIBCXX__)

// GCC std::string is *not* trivially relocatable because when the string is
// in short-size mode, the data pointer points into itself :'(

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::deque<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::forward_list<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::vector<T>);


// Specializations for Microsoft STL.
#elif defined(_MSC_VER)

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::deque<T>);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::forward_list<T>);

template<>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::string);

template<typename T>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(std::vector<T>);

#endif


#endif

}  // namespace parlay

#endif //PARLAY_TYPE_TRAITS_H_
