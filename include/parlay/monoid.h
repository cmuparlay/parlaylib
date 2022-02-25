
#ifndef PARLAY_MONOID_H_
#define PARLAY_MONOID_H_

#include <cstddef>

#include <algorithm>    // IWYU pragma: keep
#include <array>
#include <iterator>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "portability.h"
#include "type_traits.h"

namespace parlay {

// Definition of various monoids. Each consists of:
//   T identity    : returns identity for the monoid
//   T f(T&&, T&&) : adds two elements, must be associative

// ----------------------------- Type traits -------------------------------------------

// Type trait for detecting the value type of a Monoid, denoted by the type of "identity"
template<typename Monoid_>
using monoid_value_type = type_identity<decltype(std::remove_reference_t<Monoid_>::identity)>;

// Returns the value type of the given Monoid, denoted by the type of "identity"
template<typename Monoid_>
using monoid_value_type_t = typename monoid_value_type<Monoid_>::type;

// Defines the member value true if Monoid_ is a valid monoid. That is, it has
// a member "identity" of some type T which is move constructible, and is a binary
// operator over the type T
template<typename Monoid_, typename = void>
struct is_monoid : public std::false_type {};

template<typename Monoid_>
struct is_monoid<Monoid_, std::void_t<
  monoid_value_type_t<Monoid_>,
  std::enable_if_t< std::is_move_constructible_v<monoid_value_type_t<Monoid_>> >,
  std::enable_if_t< is_binary_operator_for_v<Monoid_, monoid_value_type_t<Monoid_>> >
>> : public std::true_type {};

// True if Monoid_ is a valid monoid. That is, it has a member "identity" of some
// type T which is move constructible, and a member f, which is a valid binary
// operator over the type T
template<typename Monoid_>
inline constexpr bool is_monoid_v = is_monoid<Monoid_>::value;

// Defines the member value true if Monoid_ is a valid monoid (see is_monoid)
// and the binary operator defined by the monoid is applicable to the type T
template<typename Monoid_, typename T, typename = void>
struct is_monoid_for : public std::false_type {};

template<typename Monoid_, typename T>
struct is_monoid_for<Monoid_, T, std::void_t<
  std::enable_if_t< is_monoid_v<Monoid_> >,
  std::enable_if_t< is_binary_operator_for_v<Monoid_, monoid_value_type_t<Monoid_>, T> >
>> : public std::true_type {};

// True if Monoid_ is a valid monoid (see is_monoid_v) and the binary operator
// defined by the monoid is applicable to the type T
template<typename Monoid_, typename T>
inline constexpr bool is_monoid_for_v = is_monoid_for<Monoid_, T>::value;

// ----------------------------- Definitions -------------------------------------------

// Built-in monoids with sensible default identities

template<typename T>
struct plus : public std::plus<> {
  static_assert(is_binary_operator_for_v<std::plus<>, T>);
  T identity;
  plus() : identity(0) { }
  explicit plus(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct multiplies : public std::multiplies<>  {
  static_assert(is_binary_operator_for_v<std::multiplies<>, T>);
  T identity;
  multiplies() : identity(1) { }
  explicit multiplies(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct logical_and : public std::logical_and<> {
  static_assert(is_binary_operator_for_v<std::logical_and<>, T>);
  T identity;
  logical_and() : identity(true) { }
  explicit logical_and(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct logical_or : public std::logical_or<> {
  static_assert(is_binary_operator_for_v<std::logical_or<>, T>);
  T identity;
  logical_or() : identity(false) { }
  explicit logical_or(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct bit_or : public std::bit_or<> {
  static_assert(is_binary_operator_for_v<std::bit_or<>, T>);
  T identity;
  bit_or() : identity(0) { }
  explicit bit_or(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct bit_xor : public std::bit_xor<> {
  static_assert(is_binary_operator_for_v<std::bit_xor<>, T>);
  T identity;
  bit_xor() : identity(0) { }
  explicit bit_xor(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct bit_and : public std::bit_and<> {
  static_assert(is_binary_operator_for_v<std::bit_and<>, T>);
  T identity;
  bit_and() : identity(~static_cast<T>(0)) { }
  explicit bit_and(T identity_) : identity(std::move(identity_)) { }
};

template<typename T>
struct maximum {
  T identity;
  maximum() : identity(std::numeric_limits<T>::lowest()) { }
  explicit maximum(T identity_) : identity(std::move(identity_)) { }
  template<typename T1, typename T2>
  T operator()(T1&& x, T2&& y) const { return std::max<T>(std::forward<T1>(x), std::forward<T2>(y)); }
};

template<typename T>
struct minimum {
  T identity;
  minimum() : identity(std::numeric_limits<T>::max()) { }
  explicit minimum(T identity_) : identity(std::move(identity_)) { }
  template<typename T1, typename T2>
  T operator()(T1&& x, T2&& y) const { return std::min<T>(std::forward<T1>(x), std::forward<T2>(y)); }
};

// -------------------------- Custom user-defined monoids ------------------------------

template <typename F, typename TT, typename = void>
struct monoid {
  static_assert(is_binary_operator_for_v<F, TT>);
  using T = TT;
  T identity;
  F f;
  monoid(F f, T id) : identity(std::move(id)), f(std::move(f)) { }
  template<typename T1, typename T2>
  PARLAY_INLINE decltype(auto) operator()(T1&& x, T2&& y) const {
    static_assert(std::is_invocable_r_v<T, const F&, T1, T2>);
    return std::invoke(f, std::forward<T1>(x), std::forward<T2>(y));
  }
};

template<typename F, typename TT>
struct monoid<F, TT, std::enable_if_t<std::is_class_v<F>>> : public F {
  static_assert(is_binary_operator_for_v<F, TT>);
  using T = TT;
  T identity;
  monoid(F f, T id) : F(std::move(f)), identity(std::move(id)) { }
};

template<typename F, typename T>
monoid<F, T> binary_op(F f, T id) {
  static_assert(is_binary_operator_for_v<F, T>);
  return monoid<F, T>(std::move(f), std::move(id));
}

// ----------------------------- Legacy definitions -------------------------------------------
// These are now deprecated. They remain here for backwards compatibility

// Defines the member value true if Monoid_ is a valid legacy monoid. That is, it has
// a typedef T which is move constructible, a member "identity" of type T , and a member
// f, which is a valid binary operator over the type T.
//
// Library functions that used legacy monoids have overloads that still support them,
// e.g., parlay::reduce and parlay::scan, but all new library functions will only support
// the new monoids. This ensures backward compatibility, but encourages the use of the
// new ones for new code.
template<typename Monoid_, typename = void>
struct is_legacy_monoid : public std::false_type {};

template<typename Monoid_>
struct is_legacy_monoid<Monoid_, std::void_t<
  typename std::remove_reference_t<Monoid_>::T,
  monoid_value_type_t<Monoid_>,
  decltype( std::declval<Monoid_>().f(std::declval<monoid_value_type_t<Monoid_>>(),
                                      std::declval<monoid_value_type_t<Monoid_>>()) ),
  std::enable_if_t< std::is_same_v<monoid_value_type_t<Monoid_>, typename std::remove_reference_t<Monoid_>::T> >,
  std::enable_if_t< std::is_move_constructible_v<monoid_value_type_t<Monoid_>> >,
  std::enable_if_t< std::is_convertible_v<decltype(std::declval<Monoid_>().f(
    std::declval<monoid_value_type_t<Monoid_>>(), std::declval<monoid_value_type_t<Monoid_>>()) ),
      monoid_value_type_t<Monoid_>> >
>> : public std::true_type {};

// True if Monoid_ is a valid legacy monoid. That is, it has a typedef T which is
// move constructible, a member "identity" of type T , and a member f, which is a
// valid binary operator over the type T
template<typename Monoid_>
inline constexpr bool is_legacy_monoid_v = is_legacy_monoid<Monoid_>::value;

// Takes a legacy monoid and adds an operator() to make it compatible with the
// newer monoid interface
template<typename LegacyMonoid>
struct legacy_monoid_adapter : public LegacyMonoid {
  static_assert(is_legacy_monoid_v<LegacyMonoid>);

  explicit legacy_monoid_adapter(LegacyMonoid m) : LegacyMonoid(std::move(m)) { }

  template<typename T1, typename T2> PARLAY_INLINE
  decltype(auto) operator()(T1&& x, T2&& y) { return this->f(std::forward<T1>(x), std::forward<T2>(y)); }

  template<typename T1, typename T2> PARLAY_INLINE
  decltype(auto) operator()(T1&& x, T2&& y) const { return this->f(std::forward<T1>(x), std::forward<T2>(y)); }
};

template <typename F, typename T>
monoid<F, T> make_monoid(F f, T id) {
  static_assert(is_binary_operator_for_v<F, T>);
  return monoid<F, T>(std::move(f), std::move(id));
}

template <typename M1, typename M2>
auto pair_monoid(M1 m1, M2 m2) {
  static_assert(is_legacy_monoid_v<M1>);
  static_assert(is_legacy_monoid_v<M2>);
  using P = std::pair<monoid_value_type_t<M1>, monoid_value_type_t<M2>>;
  auto identity = P(m1.identity, m2.identity);
  auto f = [m1 = std::move(m1), m2 = std::move(m2)](auto&& a, auto&& b) {
    return P(m1.f(std::get<0>(std::forward<decltype(a)>(a)), std::get<0>(std::forward<decltype(b)>(b))),
             m2.f(std::get<1>(std::forward<decltype(a)>(a)), std::get<1>(std::forward<decltype(b)>(b))));
  };
  return make_monoid(std::move(f), std::move(identity));
}

template <typename M, size_t n>
auto array_monoid(M m) {
  using Ar = std::array<monoid_value_type_t<M>, n>;
  Ar id;
  for (size_t i = 0; i < n; i++) id[i] = m.identity;
  auto f = [m = std::move(m)](Ar a, Ar b) {
    Ar r;
    for (size_t i = 0; i < n; i++) r[i] = m.f(a[i], b[i]);
    return r;
  };
  return make_monoid(std::move(f), std::move(id));
}

template <class TT>
struct addm {
  using T = TT;
  addm() : identity(0) {}
  T identity;
  static T f(T a, T b) { return a + b; }
};

template <class TT>
struct maxm {
  using T = TT;
  maxm() : identity(std::numeric_limits<T>::lowest()) {}
  T identity;
  static T f(T a, T b) { return (std::max)(a, b); }
};

template <class T1, class T2>
struct maxm<std::pair<T1, T2>> {
  using T = std::pair<T1, T2>;
  maxm() : identity(std::make_pair(std::numeric_limits<T1>::lowest(), std::numeric_limits<T2>::lowest())) {}
  T identity;
  static T f(T a, T b) { return (std::max)(a, b); }
};

template <class TT>
struct minm {
  using T = TT;
  minm() : identity((std::numeric_limits<T>::max)()) {}
  T identity;
  static T f(T a, T b) { return (std::min)(a, b); }
};

template <class T1, class T2>
struct minm<std::pair<T1, T2>> {
  using T = std::pair<T1, T2>;
  minm() : identity(std::make_pair((std::numeric_limits<T1>::max)(), (std::numeric_limits<T2>::max)())) {}
  T identity;
  static T f(T a, T b) { return (std::min)(a, b); }
};

template <class TT>
struct xorm {
  using T = TT;
  xorm() : identity(0) {}
  T identity;
  static T f(T a, T b) { return a ^ b; }
};

template <class TT>
struct minmaxm {
  using T = std::pair<TT, TT>;
  minmaxm() : identity(T((std::numeric_limits<TT>::max)(), (std::numeric_limits<TT>::lowest)())) {}
  T identity;
  static T f(T a, T b) {
    return T((std::min)(a.first, b.first), (std::max)(a.second, b.second));
  }
};


}  // namespace parlay

#endif  // PARLAY_MONOID_H_
