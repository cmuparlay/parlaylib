
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

#include "type_traits.h"

namespace parlay {

// ----------------------------- Type traits -------------------------------------------

// Type trait for detecting the value type of a Monoid, denoted by the type of "identity"
template<typename Monoid_>
using monoid_value_type = type_identity<decltype(std::remove_reference_t<Monoid_>::identity)>;

// Returns the value type of the given Monoid, denoted by the type of "identity"
template<typename Monoid_>
using monoid_value_type_t = typename monoid_value_type<Monoid_>::type;

// Type trait for detecting the function type of a Monoid, given by m.f
//
// This is actually kind of tricky, because there are three cases. All
// we know if that m.f(x,y) should work, but f could be a member function,
// a static function, or a member object with an operator().

// Static or member function case. This returns a function pointer, or a
// pointer to member function. In the second case, note that member
// functions take an additional argument (an object reference)
template<typename Monoid_, typename = void>
struct monoid_function_type : public type_identity<decltype(&std::remove_reference_t<Monoid_>::f)> {};

// Member object case
template<typename Monoid_>
struct monoid_function_type<Monoid_, std::enable_if_t<std::is_member_object_pointer_v<
  decltype(&std::remove_reference_t<Monoid_>::f)
>>> : public type_identity<decltype(std::remove_reference_t<Monoid_>::f)> {};

// Returns the function type of the given Monoid, given by m.f
template<typename Monoid_>
using monoid_function_type_t = typename monoid_function_type<Monoid_>::type;

template<typename Monoid_, typename = void>
struct is_monoid : public std::false_type {};

template<typename Monoid_>
struct is_monoid<Monoid_, std::void_t<
  monoid_value_type_t<Monoid_>,
  monoid_function_type_t<Monoid_>,
  std::enable_if_t< std::is_move_constructible_v<monoid_value_type_t<Monoid_>> >,
  std::enable_if_t< is_binary_operator_for_v<monoid_function_type_t<Monoid_>, monoid_value_type_t<Monoid_>> >
>> : public std::true_type {};

template<typename Monoid_>
inline constexpr bool is_monoid_v = is_monoid<Monoid_>::value;

template<typename Monoid_, typename T, typename = void>
struct is_monoid_for : public std::false_type {};

template<typename Monoid_, typename T>
struct is_monoid_for<Monoid_, T, std::void_t<
  std::enable_if_t< is_monoid_v<Monoid_> >,
  std::enable_if_t< is_binary_operator_for_v<monoid_function_type_t<Monoid_>, monoid_value_type_t<Monoid_>, T> >
>> : public std::true_type {};

template<typename Monoid_, typename T>
inline constexpr bool is_monoid_for_v = is_monoid_for<Monoid_, T>::value;

// ----------------------------- Definitions -------------------------------------------

// Definition of various monoids. Each consists of:
//   typename T    : type of the values
//   T identity    : returns identity for the monoid
//   T f(T&&, T&&) : adds two elements, must be associative

template <typename F, typename TT>
struct monoid {
  using T = TT;
  F f;
  T identity;
  monoid(F f, T id) : f(std::move(f)), identity(std::move(id)) { }
};

template<typename F, typename T>
monoid<F, T> binary_op(F f, T id) {
  static_assert(is_binary_operator_for_v<F, T>);
  return monoid<F, T>(std::move(f), std::move(id));
}

// ----------------------------- Old definitions -------------------------------------------
// These are now renamed / deprecated. They remain here for backwards compatibility

template <class F, class T>
monoid<F, T> make_monoid(F f, T id) {
  static_assert(is_binary_operator_for_v<F, T>);
  return monoid<F, T>(std::move(f), std::move(id));
}

template <class M1, class M2>
auto pair_monoid(M1 m1, M2 m2) {
  static_assert(is_monoid_v<M1>);
  static_assert(is_monoid_v<M2>);
  using P = std::pair<monoid_value_type_t<M1>, monoid_value_type_t<M2>>;
  auto f = [f1 = std::move(m1.f), f2 = std::move(m2.f)](auto&& a, auto&& b) {
    return P(f1(std::get<0>(std::forward<decltype(a)>(a)), std::get<0>(std::forward<decltype(b)>(b))),
             f2(std::get<1>(std::forward<decltype(a)>(a)), std::get<1>(std::forward<decltype(b)>(b))));
  };
  return make_monoid(f, P(std::move(m1.identity), std::move(m2.identity)));
}

template <class M, size_t n>
auto array_monoid(M m) {
  using Ar = std::array<typename M::T, n>;
  auto f = [&](Ar a, Ar b) {
    Ar r;
    for (size_t i = 0; i < n; i++) r[i] = m.f(a[i], b[i]);
    return r;
  };
  Ar id;
  for (size_t i = 0; i < n; i++) id[i] = m.identity;
  return make_monoid(f, id);
}

template <class TT>
struct addm {
  using T = TT;
  addm() : identity(0) {}
  T identity;
  static T f(T a, T b) { return a + b; }
};

template <class T>
T lowest() {
  return std::numeric_limits<T>::lowest();
}

template <class T>
T highest() {
  return (std::numeric_limits<T>::max)();
}

template <class TT>
struct maxm {
  using T = TT;
  maxm() : identity(lowest<T>()) {}
  T identity;
  static T f(T a, T b) { return (std::max)(a, b); }
};

template <class T1, class T2>
struct maxm<std::pair<T1, T2>> {
  using T = std::pair<T1, T2>;
  maxm() : identity(std::make_pair(lowest<T1>(), lowest<T2>())) {}
  T identity;
  static T f(T a, T b) { return (std::max)(a, b); }
};

template <class TT>
struct minm {
  using T = TT;
  minm() : identity(highest<T>()) {}
  T identity;
  static T f(T a, T b) { return (std::min)(a, b); }
};

template <class T1, class T2>
struct minm<std::pair<T1, T2>> {
  using T = std::pair<T1, T2>;
  minm() : identity(std::make_pair(highest<T1>(), highest<T2>())) {}
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
  minmaxm() : identity(T(highest<TT>(), lowest<TT>())) {}
  T identity;
  static T f(T a, T b) {
    return T((std::min)(a.first, b.first), (std::max)(a.second, b.second));
  }
};


}  // namespace parlay

#endif  // PARLAY_MONOID_H_
