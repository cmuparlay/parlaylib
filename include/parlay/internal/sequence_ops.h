
#ifndef PARLAY_SEQUENCE_OPS_H_
#define PARLAY_SEQUENCE_OPS_H_

#include <iostream>

#include "../delayed_sequence.h"
#include "../monoid.h"
#include "../range.h"
#include "../sequence.h"
#include "../utilities.h"

namespace parlay {
namespace internal {

// Return a sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename UnaryOp>
auto tabulate(size_t n, UnaryOp&& f, size_t granularity=0) {
  return sequence<typename std::remove_reference<
                  typename std::remove_cv<
                  decltype(f(0))
                  >::type>::type>::
    from_function(n, f, granularity);
}

// Return a sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto map(R&& r, UnaryOp&& f, size_t granularity=0) {
  return tabulate(parlay::size(r), [&f, it = std::begin(r)](size_t i) {
     return f(it[i]); }, granularity);
}

// Return a delayed sequence consisting of the elements
//   f(0), f(1), ... f(n)
template<typename F>
auto delayed_tabulate(size_t n, F&& f) {
  using T = decltype(f(n));
  return delayed_seq<T, F>(n, std::forward<F>(f));
}

template <class F>
auto dseq (size_t n, F f) -> delayed_sequence<decltype(f(0)),F> {
  using T = decltype(f(0));
  return delayed_sequence<T,F>(n,f);
}

// Deprecated. Renamed to delayed_map.
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto dmap(R&& r, UnaryOp&& f) {
  size_t n = parlay::size(r);
  return delayed_tabulate(n, [ r = std::forward<R>(r), f = std::forward<UnaryOp>(f) ]
      (size_t i) { return f(std::begin(r)[i]); });
}

// Return a delayed sequence consisting of the elements
//   f(r[0]), f(r[1]), ..., f(r[n-1])
//
// If r is a temporary, the delayed sequence will take
// ownership of it by moving it. If r is a reference,
// the delayed sequence will hold a reference to it, so
// r must remain alive as long as the delayed sequence.
template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto delayed_map(R&& r, UnaryOp&& f) {
  return dmap(std::forward<R>(r), std::forward<UnaryOp>(f));
}

template<PARLAY_RANGE_TYPE R, typename UnaryOp>
auto delayed_map(R &r, UnaryOp&& f) {
  size_t n = parlay::size(r);
  return delayed_tabulate(n, [ri = std::begin(r), f = std::forward<UnaryOp>(f) ]
      (size_t i) { return f(ri[i]); });
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
auto reduce_serial(Seq const &A, Monoid m) -> typename Seq::value_type {
  using T = typename Seq::value_type;
  T r = A[0];
  for (size_t j = 1; j < A.size(); j++) r = m.f(r, A[j]);
  return r;
}

template <typename Seq, typename Monoid>
auto reduce(Seq const &A, Monoid m, flags fl = no_flag) ->
    typename Seq::value_type {
  using T = typename Seq::value_type;
  size_t n = A.size();
  size_t block_size = (std::max)(_block_size, 4 * (size_t)ceil(sqrt(n)));
  size_t l = num_blocks(n, block_size);
  if (l == 0) return m.identity;
  if (l == 1 || (fl & fl_sequential)) {
    return reduce_serial(A, m);
  }
  sequence<T> Sums(l);
  sliced_for(n, block_size, [&](size_t i, size_t s, size_t e) {
    Sums[i] = reduce_serial(make_slice(A).cut(s, e), m);
  });
  T r = internal::reduce(Sums, m);
  return r;
}

const flags fl_scan_inclusive = (1 << 4);

template <typename In_Seq, typename Out_Seq, class Monoid>
auto scan_serial(In_Seq const &In, Out_Seq Out, Monoid const &m,
                 typename In_Seq::value_type offset, flags fl, bool out_uninitialized = false) ->
    typename In_Seq::value_type {
  using T = typename In_Seq::value_type;
  T r = offset;
  size_t n = In.size();
  bool inclusive = fl & fl_scan_inclusive;
  if (inclusive) {
    for (size_t i = 0; i < n; i++) {
      r = m.f(r, In[i]);
      if (out_uninitialized)
	assign_uninitialized(Out[i], r);
      else Out[i] = r;
    }
  } else {
    for (size_t i = 0; i < n; i++) {
      T t = In[i];
      if (out_uninitialized)
	assign_uninitialized(Out[i], r);
      else Out[i] = r;
      r = m.f(r, t);
    }
  }
  return r;
}

  
template <typename In_Seq, typename Out_Range, class Monoid>
auto scan_(In_Seq const &In, Out_Range Out, Monoid const &m, flags fl,
	   bool out_uninitialized=false)
    -> typename In_Seq::value_type {
  using T = typename In_Seq::value_type;
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  if (l <= 2 || fl & fl_sequential)
    return scan_serial(In, Out, m, m.identity, fl, out_uninitialized);
  sequence<T> Sums(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    Sums[i] = reduce_serial(make_slice(In).cut(s, e), m);
  });
  T total = scan_serial(Sums, make_slice(Sums), m, m.identity, 0, false);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    auto O = make_slice(Out).cut(s, e);
    scan_serial(make_slice(In).cut(s, e), O, m, Sums[i], fl, out_uninitialized);
  });
  return total;
}

template <typename Iterator, typename Monoid>
auto scan_inplace(slice<Iterator, Iterator> In, Monoid m, flags fl = no_flag) {
  return scan_(In, In, m, fl);
}

template <typename In_Seq, class Monoid>
auto scan(In_Seq const &In, Monoid m, flags fl = no_flag)
    -> std::pair<sequence<typename In_Seq::value_type>,
                 typename In_Seq::value_type> {
  using T = typename In_Seq::value_type;
  auto Out = sequence<T>::uninitialized(In.size());
  return std::make_pair(std::move(Out), scan_(In, make_slice(Out), m, fl, true));
}

// do in place if rvalue reference to a sequence<T>
template <typename T, typename Monoid>
auto scan(sequence<T> &&In, Monoid m, flags fl = no_flag)
    -> std::pair<sequence<T>, T> {
  sequence<T> Out = std::move(In);
  T total = scan_(make_slice(Out), make_slice(Out), m, fl);
  return std::make_pair(std::move(Out), total);
}

template <typename Seq>
size_t sum_bools_serial(Seq const &I) {
  size_t r = 0;
  for (size_t j = 0; j < I.size(); j++) r += I[j];
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
  sequence<size_t> Sums(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    Sums[i] = sum_bools_serial(make_slice(Fl).cut(s, e));
  });
  size_t m = scan_inplace(make_slice(Sums), addm<size_t>());
  sequence<T> Out = sequence<T>::uninitialized(m);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return Out;
}

// Pack the output to the output range.
template <typename In_Seq, typename Bool_Seq, typename Out_Seq>
size_t pack_out(In_Seq const &In, Bool_Seq const &Fl, Out_Seq Out,
                flags fl = no_flag) {
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  if (l <= 1 || fl & fl_sequential) {
    return pack_serial_at(In, make_slice(Fl).cut(0, In.size()), Out);
  }
  sequence<size_t> Sums(l);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    Sums[i] = sum_bools_serial(make_slice(Fl).cut(s, e));
  });
  size_t m = scan_inplace(make_slice(Sums), addm<size_t>());
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return m;
}

// like filter but applies g before returning result
template <typename In_Seq, typename F, typename G>
auto filter_map(In_Seq const &In, F f, G g) {
  using outT = decltype(g(In[0]));
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  auto in_mapped = delayed_seq<outT>(n, [&] (size_t i) {return g(In[i]);});

  sequence<size_t> Sums(l);
  sequence<bool> Fl(n);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t r = 0;
    for (size_t j = s; j < e; j++) r += (Fl[j] = f(In[j]));
    Sums[i] = r;
  });
  size_t m = scan_inplace(make_slice(Sums), addm<size_t>());
  sequence<outT> Out = sequence<outT>::uninitialized(m);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(in_mapped).cut(s, e),
		   make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return Out;
}

template <typename In_Seq, typename F>
auto filter(In_Seq const &In, F f) -> sequence<typename In_Seq::value_type> {
  using T = typename In_Seq::value_type;
  auto identity = [&] (T x) -> T {return x;}; // no longer needed in c++20
  return filter_map(In, f, identity);
}

template <typename In_Seq, typename F>
auto filter(In_Seq const &In, F f, flags) {
  return filter(In, f);
}

// Filter and write the output to the output range.
template <typename In_Seq, typename Out_Seq, typename F>
size_t filter_out(In_Seq const &In, Out_Seq Out, F f) {
  size_t n = In.size();
  size_t l = num_blocks(n, _block_size);
  sequence<size_t> Sums(l);
  sequence<bool> Fl(n);
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    size_t r = 0;
    for (size_t j = s; j < e; j++) r += (Fl[j] = f(In[j]));
    Sums[i] = r;
  });
  size_t m = scan_inplace(make_slice(Sums), addm<size_t>());
  sliced_for(n, _block_size, [&](size_t i, size_t s, size_t e) {
    pack_serial_at(make_slice(In).cut(s, e), make_slice(Fl).cut(s, e),
                   make_slice(Out).cut(Sums[i], (i == l - 1) ? m : Sums[i + 1]));
  });
  return m;
}

template <typename In_Seq, typename Out_Seq, typename F>
size_t filter_out(In_Seq const &In, Out_Seq Out, F f, flags) {
  return filter_out(In, Out, f);
}

template <typename Idx_Type, typename Bool_Seq>
auto pack_index(Bool_Seq const &Fl, flags fl = no_flag) {
  auto identity = [](size_t i) -> Idx_Type { return static_cast<Idx_Type>(i); };
  return pack(delayed_seq<Idx_Type>(Fl.size(), identity), Fl, fl);
}

template <typename assignment_tag, typename InIterator, typename OutIterator, typename Char_Seq>
std::pair<size_t, size_t> split_three(slice<InIterator, InIterator> In,
                                      slice<OutIterator, OutIterator> Out,
                                      Char_Seq const &Fl, flags fl = no_flag) {
  size_t n = In.size();
  if (In == Out)
    throw std::invalid_argument("In and Out cannot be the same in split_three");
  size_t l = num_blocks(n, _block_size);
  sequence<size_t> Sums0(l);
  sequence<size_t> Sums1(l);
  sliced_for(n, _block_size,
             [&](size_t i, size_t s, size_t e) {
               size_t c0 = 0;
               size_t c1 = 0;
               for (size_t j = s; j < e; j++) {
                 if (Fl[j] == 0)
                   c0++;
                 else if (Fl[j] == 1)
                   c1++;
               }
               Sums0[i] = c0;
               Sums1[i] = c1;
             },
             fl);
  size_t m0 = scan_inplace(make_slice(Sums0), addm<size_t>());
  size_t m1 = scan_inplace(make_slice(Sums1), addm<size_t>());
  sliced_for(n, _block_size,
             [&](size_t i, size_t s, size_t e) {
               size_t c0 = Sums0[i];
               size_t c1 = m0 + Sums1[i];
               size_t c2 = m0 + m1 + (s - Sums0[i] - Sums1[i]);
               for (size_t j = s; j < e; j++) {
                 if (Fl[j] == 0)
                   assign_dispatch(Out[c0++], In[j], assignment_tag());
                 else if (Fl[j] == 1)
		   assign_dispatch(Out[c1++], In[j], assignment_tag());
		 else
		   assign_dispatch(Out[c2++], In[j], assignment_tag());
               }
             },
             fl);
  return std::make_pair(m0, m1);
}

// template <typename In_Seq, typename Bool_Seq>
// auto split_two(In_Seq const &In, Bool_Seq const &Fl, flags fl = no_flag)
//     -> std::pair<sequence<typename In_Seq::value_type>, size_t> {
//   using T = typename In_Seq::value_type;
//   size_t n = In.size();
//   size_t l = num_blocks(n, _block_size);
//   sequence<size_t> Sums(l);
//   sliced_for(n, _block_size,
//              [&](size_t i, size_t s, size_t e) {
//                size_t c = 0;
//                for (size_t j = s; j < e; j++) c += (Fl[j] == false);
//                Sums[i] = c;
//              },
//              fl);
//   size_t m = scan_inplace(make_slice(Sums), addm<size_t>());
//   sequence<T> Out = sequence<T>::uninitialized(n);
//   sliced_for(n, _block_size,
//              [&](size_t i, size_t s, size_t e) {
//                size_t c0 = Sums[i];
//                size_t c1 = s + (m - c0);
//                for (size_t j = s; j < e; j++) {
//                  if (Fl[j] == false)
//                    assign_uninitialized(Out[c0++], In[j]);
//                  else
//                    assign_uninitialized(Out[c1++], In[j]);
//                }
//              },
//              fl);
//   return std::make_pair(std::move(Out), m);
// }

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_SEQUENCE_OPS_H_
