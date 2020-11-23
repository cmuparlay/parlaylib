#pragma once

#include "../sequence.h"
#include "../primitives.h"
#include "uninitialized_sequence.h"
#include "stream_delayed.h"
#include "../io.h"
#include "get_time.h"

using std::cout;
using std::endl;

namespace parlay {
namespace block_delayed {
  size_t _block_size = 10000;

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
    using range = stream_delayed::forward_delayed_sequence<flatten_iterator<seq_iter>>;
    using iterator = typename range::iterator;
    range rng;
    const parlay::sequence<IDS> sub_ranges;
  
    size_t size() const {return rng.end()-rng.begin();}
    iterator begin() {return rng.end();}
    iterator end() {return rng.begin();}
    block_delayed_sequence(parlay::sequence<IDS> sub_ranges, size_t n)
      : sub_ranges(sub_ranges),
      rng(stream_delayed::forward_delayed_sequence(flatten_iterator(sub_ranges.begin()),n)) {}
  };

  std::pair<size_t,size_t> num_blocks_and_size(size_t n) {
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
    return parlay::tabulate(num_blocks, [&] (size_t i) {
	size_t start = i * block_size;
	size_t end = std::min(start + block_size, n);
	return parlay::make_slice(S).cut(start,end);
      }, 1);
  }

  // this is only needed to satisfy the overloading rules (same as above but without const)
  template <typename Seq>
  auto make_out_iterators(Seq &S) {
    size_t n = S.end()-S.begin();
    auto [num_blocks, block_size] = num_blocks_and_size(n);
    return parlay::tabulate(num_blocks, [&] (size_t i) {
	size_t start = i * block_size;
	size_t end = std::min(start + block_size, n);
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
    auto iters = make_iterators(S);
    auto sums = parlay::map(iters, [&] (auto iter) {
	return stream_delayed::reduce(m.f, m.identity, iter);});
    auto r = parlay::scan(sums, m);
    auto iters2 = parlay::tabulate(iters.size(), [&] (size_t i) {
	return stream_delayed::scan(m.f, (r.first)[i], iters[i], inclusive);}, 1);
    return std::pair(block_delayed_sequence(iters2, S.size()), r.second);
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
  long reduce(block_delayed_sequence<IDS> A, Monoid m) {
    auto sums = parlay::map(A.sub_ranges, [&] (auto iter) {
	return stream_delayed::reduce(m.f, m.identity, iter);});
    return parlay::reduce(sums, m);
  }

  template <typename Seq1, typename Seq2>
  auto zip(Seq1 const &S1, Seq2 const &S2) {
    auto iters1 = make_iterators(S1);
    auto iters2 = make_iterators(S2);
    auto results = parlay::tabulate(iters1.size(), [&] (size_t i) {
	return stream_delayed::zip(iters1[i], iters2[i]);}, 1);
    return block_delayed_sequence(results, S1.size());
  }
  
  template <typename Seq1, typename Seq2, typename F>
  auto zip_with(Seq1 const &S1, Seq2 const &S2, F f) {
    auto iters1 = make_iterators(S1);
    auto iters2 = make_iterators(S2);
    auto results = parlay::tabulate(iters1.size(), [&] (size_t i) {
	return stream_delayed::zip_with(iters1[i], iters2[i], f);}, 1);
    return block_delayed_sequence(results,S1.size());
  }

  template <typename IDS, typename F>
  void apply(block_delayed_sequence<IDS> A, F f) {
    auto I = A.sub_ranges;
    parlay::parallel_for(0, I.size(), [&] (size_t i) {
	stream_delayed::apply(I[i], f);}, 1);
  }

  template <typename Seq1, typename Seq2, typename F>
  void zip_apply(Seq1 s1, Seq2 s2, F f) {
    auto iters1 = make_iterators(s1);
    auto iters2 = make_iterators(s2);
    parlay::parallel_for(0, iters1.size(), [&] (size_t i) {
	stream_delayed::zip_apply(iters1[i], iters2[i], f);}, 1);
  }

  template <typename IDS, typename F>
  auto map(block_delayed_sequence<IDS> A, F f) {
    auto I = A.sub_ranges;
    auto results = parlay::tabulate(I.size(), [&] (size_t i) {
	return stream_delayed::map(I[i], f);
      },1);
    return block_delayed_sequence(results, A.size());
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
    using T = typename Seq::value_type::value_type;
    auto sizes = parlay::map(seq, [] (auto const& s) -> size_t {
	return s.size();});
    auto [offsets, n] = parlay::scan(sizes);
    auto [num_blocks, block_size] = num_blocks_and_size(n);
    auto results = parlay::tabulate(num_blocks, [&] (size_t i) {
	size_t start = i * block_size;
	size_t len = std::min(block_size, n-start);
	size_t j = (std::upper_bound(offsets.begin(),offsets.end(),start)
		    - offsets.begin() - 1);
	out_iter_t out_iter = seq.begin()+j;
	in_iter_t in_iter = (*out_iter).begin() + (start - offsets[j]);
	return stream_delayed::forward_delayed_sequence(flatten_iterator(in_iter, out_iter), len);
      }, 1);
    return block_delayed_sequence(results, n);
  }

  // Allocates a small temp sequence per block, and then copies them
  // into a smaller block sequenc of just copied elements, and finally into the result.
  // This involves an extra copy but works well if the temp block is reused
  // across blocks since it will be in the cache.   It can save significant memory.
  template <typename Seq, typename F>
  auto filter(Seq A, F f) {
    timer t("new filter", false);
    using T = decltype(*(A.begin()));
    size_t n = A.size();
    auto iters = make_iterators(A);
    auto num_blocks = iters.size();
    auto seqs = parlay::tabulate(num_blocks, [&] (size_t i) -> parlay::sequence<T> {
	return stream_delayed::filter(iters[i], f);}, 1);
    t.next("tabulate");
    auto sizes = parlay::map(seqs, [&] (parlay::sequence<T> const& x) {
				    return x.size();});
    auto [r, total]  = parlay::scan(sizes);
    parlay::sequence<T> out = parlay::sequence<T>::uninitialized(total);
    parlay::parallel_for(0, num_blocks, [&] (size_t i) {
	for (size_t j = 0; j < sizes[i]; j++) 
	  out[r[i]+j] = seqs[i][j];
      }, 1);
    t.next("parallel for");
    return out;
  }
}
}