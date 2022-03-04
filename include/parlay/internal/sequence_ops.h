
#ifndef PARLAY_SEQUENCE_OPS_H_
#define PARLAY_SEQUENCE_OPS_H_

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <type_traits>
#include <utility>

#include "../delayed_sequence.h"
#include "../monoid.h"
#include "../parallel.h"
#include "../range.h"
#include "../sequence.h"
#include "../slice.h"
#include "../utilities.h"

namespace parlay {
namespace internal {

// Return a sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename UnaryOp>
auto tabulate(size_t n, UnaryOp&& f, size_t granularity=0) {
  static_assert(std::is_invocable_v<UnaryOp, size_t>);
  static_assert(!std::is_void_v<std::invoke_result_t<UnaryOp, size_t>>);
  static_assert(!std::is_array_v<std::invoke_result_t<UnaryOp, size_t>>);
  return sequence<typename std::decay_t<std::invoke_result_t<UnaryOp, size_t>>>
    ::from_function(n, std::forward<UnaryOp>(f), granularity);
}

// Return a sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename T, typename UnaryOp>
auto tabulate(size_t n, UnaryOp&& f, size_t granularity=0) {
  static_assert(std::is_invocable_v<UnaryOp, size_t>);
  static_assert(!std::is_void_v<std::invoke_result_t<UnaryOp, size_t>>);
  static_assert(!std::is_array_v<std::invoke_result_t<UnaryOp, size_t>>);
  static_assert(std::is_convertible_v<std::invoke_result_t<UnaryOp, size_t>, T>);
  return sequence<T>::from_function(n, std::forward<UnaryOp>(f), granularity);
}

// Return a sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
template<typename R, typename UnaryOp>
auto map(R&& r, UnaryOp&& f, size_t granularity=0) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<UnaryOp, range_reference_type_t<R>>);
  static_assert(!std::is_void_v<std::invoke_result_t<UnaryOp, range_reference_type_t<R>>>);
  return tabulate(parlay::size(r), [f = std::forward<UnaryOp>(f), it = std::begin(r)]
      (size_t i) -> decltype(auto) { return f(it[i]); }, granularity);
}

// Return a delayed sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename F>
auto delayed_tabulate(size_t n, F f) {
  static_assert(std::is_invocable_v<const F&, size_t>);
  using T = std::invoke_result_t<const F&, size_t>;
  static_assert(!std::is_void_v<T>);
  using V = std::remove_cv_t<std::remove_reference_t<T>>;
  return delayed_sequence<T, V, F>(n, std::move(f));
}

// Return a delayed sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename T, typename F>
auto delayed_tabulate(size_t n, F f) {
  static_assert(std::is_invocable_v<const F&, size_t>);
  static_assert(std::is_convertible_v<std::invoke_result_t<const F&, size_t>, T>);
  return delayed_sequence<T, std::remove_cv_t<std::remove_reference_t<T>>, F>(n, std::move(f));
}

// Return a delayed sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename T, typename V, typename F>
auto delayed_tabulate(size_t n, F f) {
  static_assert(std::is_invocable_v<const F&, size_t>);
  static_assert(std::is_convertible_v<std::invoke_result_t<const F&, size_t>, T>);
  return delayed_sequence<T, V, F>(n, std::move(f));
}

// Return a delayed sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
//
// If r is a temporary, the delayed sequence will take
// ownership of it by moving it. If r is a reference,
// the delayed sequence will hold a reference to it, so
// r must remain alive as long as the delayed sequence.
template<typename R, typename UnaryOp,
    std::enable_if_t<std::is_rvalue_reference_v<R&&>, int> = 0>
auto delayed_map(R&& r, UnaryOp f) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<const UnaryOp&, range_reference_type_t<R>>);
  static_assert(!std::is_void_v<std::invoke_result_t<const UnaryOp&, range_reference_type_t<R>>>);

  // The closure object keeps a cache of the begin iterator to the range r.
  //
  // This is useful because some ranges might perform something like small-size optimization
  // where calling std::begin(r) could have overhead. By keeping the iterator cached, we can
  // random-access from it in the lambda without experiencing this overhead every time
  struct closure {
    closure(R&& r_) : r(std::move(r_)), it(std::begin(r)) {}
    closure(const closure& other) : r(other.r), it(std::begin(r)) {}
    closure(closure&& other) noexcept(std::is_nothrow_move_constructible_v<R>)
        : r(std::move(other.r)), it(std::begin(r)) {}
    closure& operator=(const closure& other) { r = other.r; it = std::begin(r); return *this; }
    closure& operator=(closure&& other) noexcept(std::is_nothrow_move_assignable_v<R>) {
      r = std::move(other.r); it = std::begin(r); return *this; }
    R r;
    range_iterator_type_t<R> it;
  };

  size_t n = parlay::size(r);
  return delayed_tabulate(n, [ c = closure(std::forward<R>(r)), f = std::move(f) ]
      (size_t i) -> decltype(auto) { return f(c.it[i]); });
}

template<typename R, typename UnaryOp,
    std::enable_if_t<std::is_lvalue_reference_v<R&&>, int> = 0>
auto delayed_map(R&& r, UnaryOp f) {
  static_assert(is_random_access_range_v<R>);
  static_assert(std::is_invocable_v<const UnaryOp&, range_reference_type_t<R>>);
  static_assert(!std::is_void_v<std::invoke_result_t<const UnaryOp&, range_reference_type_t<R>>>);

  size_t n = parlay::size(r);
  return delayed_tabulate(n, [ri = std::begin(r), f = std::move(f) ]
      (size_t i) -> decltype(auto) { return f(ri[i]); });
}

// Renamed. Use delayed_tabulate
template <typename F>
auto dseq (size_t n, F f) {
  return delayed_tabulate(n, std::move(f));
}

// Renamed. Use delayed_map.
template<typename R, typename UnaryOp>
auto dmap(R&& r, UnaryOp f) {
  return delayed_map(std::forward<R>(r), std::move(f));
}

template <typename T>
auto singleton(T const &v) -> sequence<T> {
  return sequence<T>(1, v);
}

template <typename Seq, typename Range>
auto copy(Seq const &A, Range R, flags) -> void {
  parallel_for(0, A.size(), [&](size_t i) { R[i] = A[i]; });
}

constexpr const size_t _log_block_size = 10;
constexpr const size_t _block_size = (1 << _log_block_size);

inline size_t num_blocks(size_t n, size_t block_size) {
  if (n == 0)
    return 0;
  else
    return (1 + ((n)-1) / (block_size));
}

template <typename F>
void sliced_for(size_t n, size_t block_size, const F &f, flags fl = no_flag) {
  size_t l = num_blocks(n, block_size);
  auto body = [&](size_t i) {
    size_t s = i * block_size;
    size_t e = (std::min)(s + block_size, n);
    f(i, s, e);
  };
  parallel_for(0, l, body, 1, 0 != (fl & fl_conservative));
}

template <typename Seq, typename Monoid>
auto reduce_serial(Seq const &A, Monoid&& m) {
  static_assert(is_random_access_range_v<Seq>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<Seq>>);
  using T = monoid_value_type_t<Monoid>;
  if (A.size() == 0) return m.identity;
  T r = A[0];
  for (size_t j = 1; j < A.size(); j++) {
    r = m(std::move(r), A[j]);
  }
  return r;
}

template <typename Seq, typename Monoid>
auto reduce(Seq const &A, Monoid&& m, flags fl = no_flag) {
  static_assert(is_random_access_range_v<Seq>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<Seq>>);
  using T = monoid_value_type_t<Monoid>;
  size_t n = A.size();
  size_t block_size = (std::max)(_block_size, 4 * static_cast<size_t>(std::ceil(std::sqrt(n))));
  size_t l = num_blocks(n, block_size);
  if (l == 0) return m.identity;
  if (l == 1 || (fl & fl_sequential)) {
    return reduce_serial(A, m);
  }
  auto sums = sequence<T>::uninitialized(l);
  sliced_for(n, block_size, [&](size_t i, size_t s, size_t e) {
    assign_uninitialized(sums[i], reduce_serial(make_slice(A).cut(s, e), m));
  });
  T r = internal::reduce(sums, m);
  return r;
}

const flags fl_scan_inclusive = (1 << 4);

template <typename In_Seq, typename Out_Seq, typename Monoid>
auto scan_serial(In_Seq const &In, Out_Seq Out, Monoid&& m,
                 monoid_value_type_t<Monoid> offset, flags fl, bool out_uninitialized = false) {
  static_assert(is_random_access_range_v<In_Seq>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<In_Seq>>);
  using T = monoid_value_type_t<Monoid>;
  T r = std::move(offset);
  size_t n = In.size();
  bool inclusive = fl & fl_scan_inclusive;
  if (inclusive) {
    for (size_t i = 0; i < n; i++) {
      r = m(std::move(r), In[i]);
      if (out_uninitialized) assign_uninitialized(Out[i], r);
      else Out[i] = r;
    }
  } else {
    for (size_t i = 0; i < n; i++) {
      T t = In[i];
      if (out_uninitialized) assign_uninitialized(Out[i], r);
      else Out[i] = r;
      r = m(std::move(r), t);
    }
  }
  return r;
}

  
template <typename In_Seq, typename Out_Range, class Monoid>
auto scan_(In_Seq const &In, Out_Range Out, Monoid&& m, flags fl, bool out_uninitialized=false) {
  static_assert(is_random_access_range_v<In_Seq>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<In_Seq>>);
  using T = monoid_value_type_t<Monoid>;
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  if (l <= 2 || fl & fl_sequential)
    return scan_serial(In, Out, m, m.identity, fl, out_uninitialized);
  auto sums = sequence<T>::uninitialized(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    assign_uninitialized(sums[i], reduce_serial(make_slice(In).cut(s, e), m));
  });
  T total = scan_serial(sums, make_slice(sums), m, m.identity, 0, false);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    auto O = make_slice(Out).cut(s, e);
    scan_serial(make_slice(In).cut(s, e), O, m, sums[i], fl, out_uninitialized);
  });
  return total;
}

template <typename Iterator, typename Monoid>
auto scan_inplace(slice<Iterator, Iterator> In, Monoid&& m, flags fl = no_flag) {
  static_assert(is_monoid_for_v<Monoid, iterator_reference_type_t<Iterator>>);
  return scan_(In, In, std::forward<Monoid>(m), fl);
}

template <typename In_Seq, typename Monoid>
auto scan(In_Seq const &In, Monoid&& m, flags fl = no_flag) {
  static_assert(is_random_access_range_v<In_Seq>);
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<In_Seq>>);
  using T = monoid_value_type_t<Monoid>;
  auto Out = sequence<T>::uninitialized(In.size());
  return std::make_pair(std::move(Out), scan_(In, make_slice(Out), std::forward<Monoid>(m), fl, true));
}

// do in place if rvalue reference to a sequence<T>
template <typename T, typename Monoid,
    std::enable_if_t<std::is_same_v<T, monoid_value_type_t<Monoid>>, int> = 0>
auto scan(sequence<T>&& In, Monoid&& m, flags fl = no_flag) {
  static_assert(is_monoid_v<Monoid>);
  sequence<T> Out = std::move(In);
  T total = scan_(make_slice(Out), make_slice(Out), std::forward<Monoid>(m), fl);
  return std::make_pair(std::move(Out), total);
}

template <typename Seq>
size_t sum_bools_serial(Seq const &I) {
  size_t r = 0;
  for (size_t j = 0; j < I.size(); j++) {
    r += static_cast<bool>(I[j]);
  }
  return r;
}

template <typename In_Seq, typename Bool_Seq>
auto pack_serial(In_Seq const &In, Bool_Seq const &Fl)
    -> sequence<typename In_Seq::value_type> {
  using T = typename In_Seq::value_type;
  size_t n = In.size();
  size_t m = sum_bools_serial(Fl);
  sequence<T> Out = sequence<T>::uninitialized(m);
  size_t k = 0;
  for (size_t i = 0; i < n; i++)
    if (Fl[i]) assign_uninitialized(Out[k++], In[i]);
  return Out;
}


template <class Slice, class Slice2, typename Out_Seq>
size_t pack_serial_at(Slice In, Slice2 Fl, Out_Seq Out) {
  size_t k = 0;
  for (size_t i = 0; i < In.size(); i++)
    if (Fl[i]) assign_uninitialized(Out[k++], In[i]);
  return k;
}

template <typename In_Seq, typename Bool_Seq>
auto pack(In_Seq const &In, Bool_Seq const &Fl, flags fl = no_flag)
    -> sequence<typename In_Seq::value_type> {
  using T = typename In_Seq::value_type;
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  if (l == 1 || fl & fl_sequential) return pack_serial(In, Fl);
  auto sums = sequence<size_t>::uninitialized(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    assign_uninitialized(sums[i], sum_bools_serial(make_slice(Fl).cut(s, e)));
  });
  size_t m = scan_inplace(make_slice(sums), plus<size_t>());
  sequence<T> Out = sequence<T>::uninitialized(m);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(sums[i], (i == l - 1) ? m : sums[i + 1]));
  });
  return Out;
}

// Pack the output to the output range.
template <typename In_Seq, typename Bool_Seq, typename Out_Seq>
size_t pack_out(In_Seq const &In, Bool_Seq const &Fl, /* uninitialized */ Out_Seq Out, flags fl = no_flag) {
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  if (l <= 1 || fl & fl_sequential) {
    return pack_serial_at(In, make_slice(Fl).cut(0, In.size()), Out);
  }
  sequence<size_t> Sums(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    Sums[i] = sum_bools_serial(make_slice(Fl).cut(s, e));
  });
  size_t m = scan_inplace(make_slice(Sums), plus<size_t>());
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return m;
}

// like filter but applies g before returning result
template <typename In_Seq, typename F, typename G>
auto filter_map(In_Seq const &In, F&& f, G&& g) {
  using outT = std::invoke_result_t<G, range_reference_type_t<In_Seq>>;
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  auto in_mapped = delayed_seq<outT>(n, [&] (size_t i) { return g(In[i]); });

  sequence<size_t> Sums(l);
  sequence<bool> Fl(n);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t r = 0;
    for (size_t j = s; j < e; j++) r += (Fl[j] = f(In[j]));
    Sums[i] = r;
  });
  size_t m = scan_inplace(make_slice(Sums), plus<size_t>());
  sequence<outT> Out = sequence<outT>::uninitialized(m);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(in_mapped).cut(s, e),
		   make_slice(Fl).cut(s, e), make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return Out;
}

template <typename In_Seq, typename F>
auto filter(In_Seq const &In, F&& f) -> sequence<typename In_Seq::value_type> {
  auto identity = [](auto&& x) -> range_value_type_t<In_Seq> { return std::forward<decltype(x)>(x); };
  return filter_map(In, std::forward<F>(f), identity);
}

template <typename In_Seq, typename F>
auto filter(In_Seq const &In, F&& f, flags) {
  return filter(In, std::forward<F>(f));
}

// Filter and write the output to the output range.
template <typename In_Seq, typename Out_Seq, typename F>
size_t filter_out(In_Seq const &In, /* uninitialized */ Out_Seq Out, F&& f) {
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  sequence<size_t> Sums(l);
  sequence<bool> Fl(n);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t r = 0;
    for (size_t j = s; j < e; j++) r += (Fl[j] = f(In[j]));
    Sums[i] = r;
  });
  size_t m = scan_inplace(make_slice(Sums), plus<size_t>());
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return m;
}

template <typename In_Seq, typename Out_Seq, typename F>
size_t filter_out(In_Seq const &In, /* uninitialized */ Out_Seq Out, F&& f, flags) {
  return filter_out(In, Out, std::forward<F>(f));
}

template <typename Idx_Type, typename Bool_Seq>
auto pack_index(Bool_Seq const &Fl, flags fl = no_flag) {
  auto identity = [](size_t i) -> Idx_Type { return static_cast<Idx_Type>(i); };
  return pack(delayed_tabulate(Fl.size(), identity), Fl, fl);
}

template <typename assignment_tag, typename InIterator, typename OutIterator, typename Char_Seq>
std::pair<size_t, size_t> split_three(slice<InIterator, InIterator> In,
                                      slice<OutIterator, OutIterator> Out,
                                      Char_Seq const &Fl, flags fl = no_flag) {
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  sequence<size_t> Sums0(l);
  sequence<size_t> Sums1(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t c0 = 0;
    size_t c1 = 0;
    for (size_t j = s; j < e; j++) {
      if (Fl[j] == 0) c0++;
      else if (Fl[j] == 1) c1++;
    }
    Sums0[i] = c0;
    Sums1[i] = c1;
  }, fl);
  size_t m0 = scan_inplace(make_slice(Sums0), plus<size_t>());
  size_t m1 = scan_inplace(make_slice(Sums1), plus<size_t>());
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t c0 = Sums0[i];
    size_t c1 = m0 + Sums1[i];
    size_t c2 = m0 + m1 + (s - Sums0[i] - Sums1[i]);
    for (size_t j = s; j < e; j++) {
      if (Fl[j] == 0) {
        assign_dispatch(Out[c0++], In[j], assignment_tag());
      } else if (Fl[j] == 1) {
        assign_dispatch(Out[c1++], In[j], assignment_tag());
      } else {
        assign_dispatch(Out[c2++], In[j], assignment_tag());
      }
    }
  }, fl);
  return std::make_pair(m0, m1);
}

template <typename In_Seq, typename Bool_Seq>
auto split_two(In_Seq const &In, Bool_Seq const &Fl, flags fl = no_flag)
    -> std::pair<sequence<typename In_Seq::value_type>, size_t> {
  using T = typename In_Seq::value_type;
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  sequence<size_t> Sums(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t c = 0;
    for (size_t j = s; j < e; j++) c += (Fl[j] == false);
    Sums[i] = c;
  }, fl);
  size_t m = scan_inplace(make_slice(Sums), plus<size_t>());
  sequence<T> Out = sequence<T>::uninitialized(n);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t c0 = Sums[i];
    size_t c1 = s + (m - c0);
    for (size_t j = s; j < e; j++) {
      if (Fl[j] == false)
        assign_uninitialized(Out[c0++], In[j]);
      else
        assign_uninitialized(Out[c1++], In[j]);
    }
  }, fl);
  return std::make_pair(std::move(Out), m);
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_SEQUENCE_OPS_H_
