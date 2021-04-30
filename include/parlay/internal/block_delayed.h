#ifndef PARLAY_INTERNAL_BLOCK_DELAYED_H_
#define PARLAY_INTERNAL_BLOCK_DELAYED_H_

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../delayed_sequence.h"
#include "../monoid.h"
#include "../parallel.h"
#include "../sequence.h"
#include "../slice.h"

#include "get_time.h"
#include "sequence_ops.h"
#include "stream_delayed.h"

namespace parlay {
namespace block_delayed {

static size_t _block_size = 2000;

// takes nested forward iterators and flattens them into a single forward iterator
// The value_type of the outer iterator must be a range type
// containing value_type and iterator types, and supporting begin(), and end().
// Used both in block_delayed_sequence to get a single iterator that goes
// through all blocks, and in flatten, to get a range over multiple blocks.
template <typename out_iter_t>
struct flatten_iterator {
  using in_range_t = typename std::iterator_traits<out_iter_t>::value_type;
  using in_iter_t = typename in_range_t::iterator;
  using value_type = typename in_range_t::value_type;
  in_iter_t in_iter;
  out_iter_t out_iter;
  flatten_iterator& operator++() {
    ++in_iter;
    if (in_iter == (*out_iter).end()) {// if at end of inner range
      ++out_iter;                      // move to next outer
      in_iter = (*out_iter).begin();   // and set inner to start
    }
    return *this;
  }
  value_type operator*() const {return *in_iter;}
  flatten_iterator(in_iter_t in_iter, out_iter_t out_iter) :
      in_iter(in_iter), out_iter(out_iter) {}
  flatten_iterator(out_iter_t out_iter) :
      in_iter((*out_iter).begin()), out_iter(out_iter) {}
};

template <typename IDS>
struct block_delayed_sequence {
  using value_type = typename parlay::sequence<IDS>::value_type;
  using seq_iter = typename parlay::sequence<IDS>::iterator;
  using range_type = stream_delayed::forward_delayed_sequence<flatten_iterator<seq_iter>>;
  using iterator = typename range_type::iterator;


  size_t size() const {return rng.end()-rng.begin();}
  iterator begin() {return rng.end();}
  iterator end() {return rng.begin();}
  block_delayed_sequence(parlay::sequence<IDS> sub_ranges_, size_t n)
      : sub_ranges(std::move(sub_ranges_)),
        rng(stream_delayed::forward_delayed_sequence(flatten_iterator(sub_ranges.begin()),n)) {}

  parlay::sequence<IDS> sub_ranges;
  range_type rng;
};

static inline std::pair<size_t,size_t> num_blocks_and_size(size_t n) {
  return std::pair((n==0) ? 0 : 1ul + ((n)-1ul) / (_block_size),
                   _block_size);
}

// helpers to check if a type is a delayed sequence
template <typename>
struct is_delayed_base : std::false_type {};
template <typename IDS>
struct is_delayed_base<block_delayed_sequence<IDS>> : std::true_type {};
template <typename T, typename F>
struct is_delayed_base<parlay::delayed_sequence<T,F>> : std::true_type {};
template<typename T>
struct is_delayed : is_delayed_base<std::remove_cv_t<T>> {};


template <typename Seq>
auto make_iterators(Seq const &S) {
  size_t n = S.end()-S.begin();
  auto [num_blocks, block_size] = num_blocks_and_size(n);
  return internal::tabulate(num_blocks, [&, bs = block_size] (size_t i) {
    size_t start = i * bs;
    size_t end = (std::min)(start + bs, n);
    return parlay::make_slice(S).cut(start,end);
  }, 1);
}

// this is only needed to satisfy the overloading rules (same as above but without const)
template <typename Seq>
auto make_out_iterators(Seq &S) {
  size_t n = S.end()-S.begin();
  auto [num_blocks, block_size] = num_blocks_and_size(n);
  return internal::tabulate(num_blocks, [&, bs = block_size] (size_t i) {
    size_t start = i * bs;
    size_t end = (std::min)(start + bs, n);
    return parlay::make_slice(S).cut(start,end);
  }, 1);
}

template <typename IDS>
auto make_iterators(block_delayed_sequence<IDS> const &A) {
  return A.sub_ranges;
}

// ***************************
// User functions below here
// ***************************

template <typename Seq, typename Monoid>
auto scan_(Seq const &S, Monoid const &m, bool inclusive) {
  using T = typename Seq::value_type;
  auto iters = make_iterators(S);
  size_t num_blocks = iters.size();
  sequence<T> offsets;
  T total = m.identity;
  // if only one block and inclusive then can skip first phase
  if (!(num_blocks == 1 && inclusive)) {
    auto sums = internal::map(iters, [&] (auto iter) {
      return stream_delayed::reduce(m.f, m.identity, iter);});
    std::tie(offsets, total) = internal::scan(sums, m);
  } else offsets = sequence<T>(1, m.identity);
  auto iters2 = internal::tabulate(num_blocks, [&] (size_t i) {
    return stream_delayed::scan(m.f, offsets[i], iters[i], inclusive);}, 1);
  assert(!(iters2.empty()));
  auto bls = block_delayed_sequence(std::move(iters2), S.size());
  return std::pair(std::move(bls), inclusive ? m.identity : total);
}

// does not work if input is parlay-delayed for some reason
template <typename Seq, typename Monoid>
auto scan(Seq const &S, Monoid const &m) {
  return scan_(S, m, false);}

// does not work if input is parlay-delayed for some reason
template <typename Seq, typename Monoid>
auto scan_inclusive(Seq const &S, Monoid const &m) {
  return scan_(S, m, true).first;}

template <typename IDS, typename Monoid>
auto reduce(block_delayed_sequence<IDS> const &A, Monoid const &m) {
  auto sums = internal::map(A.sub_ranges, [&] (auto iter) {
    return stream_delayed::reduce(m.f, m.identity, iter);});
  return internal::reduce(sums, m);
}

template <typename Seq1, typename Seq2>
auto zip(Seq1 const &S1, Seq2 const &S2) {
  size_t n = S1.size();
  auto iters1 = make_iterators(S1);
  auto iters2 = make_iterators(S2);
  auto results = internal::tabulate(iters1.size(), [&] (size_t i) {
    return stream_delayed::zip(iters1[i], iters2[i]);}, 1);
  return block_delayed_sequence(std::move(results), n);
}

template <typename Seq1, typename Seq2, typename F>
auto zip_with(Seq1 const &S1, Seq2 const &S2, F const &f) {
  auto iters1 = make_iterators(S1);
  auto iters2 = make_iterators(S2);
  auto results = internal::tabulate(iters1.size(), [&] (size_t i) {
    return stream_delayed::zip_with(iters1[i], iters2[i], f);}, 1);
  return block_delayed_sequence(std::move(results),S1.size());
}

template <typename IDS, typename F>
void apply(block_delayed_sequence<IDS> const &A, F const &f) {
  auto I = A.sub_ranges;
  parlay::parallel_for(0, I.size(), [&] (size_t i) {
    stream_delayed::apply(I[i], f);}, 1);
}

template <typename Seq1, typename Seq2, typename F>
void zip_apply(Seq1 const &s1, Seq2 const &s2, F const &f) {
  auto iters1 = make_iterators(s1);
  auto iters2 = make_iterators(s2);
  parlay::parallel_for(0, iters1.size(), [&] (size_t i) {
    stream_delayed::zip_apply(iters1[i], iters2[i], f);}, 1);
}

template <typename IDS, typename F>
auto map(block_delayed_sequence<IDS> const &A, F const &f) {
  auto I = A.sub_ranges;
  auto results = internal::tabulate(I.size(), [&] (size_t i) {
    return stream_delayed::map(I[i], f);
  },1);
  return block_delayed_sequence(std::move(results), A.size());
}

template <typename Seq,
    typename std::enable_if_t<is_delayed<Seq>::value, int> = 0>
auto force(Seq A) {
  size_t n = A.size();
  using T = decltype(*(A.begin()));
  auto idx = parlay::delayed_seq<size_t>(n, [] (size_t i) {return i;});
  auto r = parlay::sequence<T>::uninitialized(n);
  auto f = [&] (size_t i, T a) {r[i] = a;};
  block_delayed::zip_apply(idx, A, f);
  return r;
}

// force is the identity if not a delayed sequence
template <typename Seq,
    typename std::enable_if_t<!is_delayed<Seq>::value, int> = 0>
auto force(Seq &A) {return A;}

template<typename Seq>
auto flatten(Seq &seq) {
  using out_iter_t = typename Seq::iterator;
  using in_iter_t = typename Seq::value_type::iterator;
  //using T = typename Seq::value_type::value_type;
  auto sizes = internal::map(seq, [] (auto const& s) -> size_t {
    return s.size();});
  auto res = internal::scan(sizes,addm<size_t>());
  auto offsets = res.first;
  auto n = res.second;
  auto [num_blocks, block_size] = num_blocks_and_size(n);
  auto results = internal::tabulate(num_blocks, [&, block_size=block_size] (size_t i) {
    size_t start = i * block_size;
    size_t len = std::min(block_size, n - start);
    size_t j = (std::upper_bound(offsets.begin(),offsets.end(),start)
                - offsets.begin() - 1);
    out_iter_t out_iter = seq.begin()+j;
    in_iter_t in_iter = (*out_iter).begin() + (start - offsets[j]);
    return stream_delayed::forward_delayed_sequence(flatten_iterator(in_iter, out_iter), len);
  }, 1);
  return block_delayed_sequence(std::move(results), n);
}

// Allocates a small temp sequence per block, and then copies them
// into a smaller block sequenc of just copied elements, and finally into the result.
// This involves an extra copy but works well if the temp block is reused
// across blocks since it will be in the cache.   It can save significant memory.
template <typename Seq, typename F, typename G>
auto filter_map(Seq A, F const &f, G const &g) {
  internal::timer t("new filter", false);
  using T = decltype(g(*(A.begin())));
  auto iters = make_iterators(A);
  auto num_blocks = iters.size();
  if (num_blocks == 1)
    return stream_delayed::filter_map(iters[0], f, g);
  auto seqs = internal::tabulate(num_blocks, [&] (size_t i) -> parlay::sequence<T> {
    return stream_delayed::filter_map(iters[i], f, g);}, 1);
  t.next("tabulate");
  auto sizes = internal::map(seqs, [&] (parlay::sequence<T> const& x) {
    return x.size();});
  auto [r, total]  = internal::scan(sizes, addm<size_t>());
  parlay::sequence<T> out = parlay::sequence<T>::uninitialized(total);
  parlay::parallel_for(0, num_blocks, [&, ri = r.begin(), oi = out.begin()] (size_t i) {
    for (size_t j = 0; j < sizes[i]; j++)
      oi[ri[i]+j] = seqs[i][j];
  }, 1);
  t.next("parallel for");
  return out;
}

template <typename Seq, typename F>
auto filter(Seq const &A, F const &f) {
  return block_delayed::filter_map(A, f, [] (auto x) {return x;});
}

}  // namespace block_delayed
}  // namespace parlay

#endif  // PARLAY_INTERNAL_BLOCK_DELAYED_H_
