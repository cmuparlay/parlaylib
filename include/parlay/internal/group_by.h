#ifndef PARLAY_INTERNAL_GROUP_BY_H_
#define PARLAY_INTERNAL_GROUP_BY_H_

#include <cstdlib>

#include <functional>
#include <utility>

#include "../monoid.h"
#include "../primitives.h"
#include "../range.h"
#include "../sequence.h"
#include "../slice.h"
#include "../utilities.h"

#include "block_delayed.h"
#include "collect_reduce.h"
#include "counting_sort.h"

namespace parlay {

// Clang gives false positives for unused type
// aliases inside the nested helper structs
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif

template <PARLAY_RANGE_TYPE Range, typename Comp>
auto group_by_key_sorted(const Range& S, const Comp& less) {
  using KV = range_value_type_t<Range>;
  using K = typename KV::first_type;
  using V = typename KV::second_type;
  size_t n = S.size();

  sequence<KV> sorted;
  if constexpr(std::is_integral_v<K> &&
               std::is_unsigned_v<K> &&
               sizeof(KV) <= 16) {
    sorted = parlay::integer_sort(make_slice(S), [] (KV a) { return a.first; });
  } else {
    auto pair_less = [&] (auto const &a, auto const &b) {
      return less(a.first, b.first);};
    sorted = parlay::stable_sort(make_slice(S), pair_less);
  }

  auto vals = sequence<V>::uninitialized(n);

  auto idx = block_delayed::filter(iota(n), [&] (size_t i) {
    assign_uninitialized(vals[i], sorted[i].second);
    return (i==0) || less(sorted[i-1].first, sorted[i].first);});

  auto r = tabulate(idx.size(), [&] (size_t i) {
    size_t start = idx[i];
    size_t end = ((i==idx.size()-1) ? n : idx[i+1]);
    return std::pair(std::move(sorted[idx[i]].first),
                     to_sequence(vals.cut(start, end)));
  });
  return r;
}

template <PARLAY_RANGE_TYPE Range>
auto group_by_key_sorted(const Range& S) {
  return group_by_key_sorted(S, std::less{});
}


template <typename arg_type, typename Monoid, typename Hash, typename Equal>
struct reduce_by_key_helper {
  using in_type = arg_type;
  using key_type = typename in_type::first_type;
  using value_type = typename in_type::second_type;
  using result_type = in_type;
  Monoid monoid; Hash hash; Equal equal;
  reduce_by_key_helper(Monoid const &m, Hash const &h, Equal const &e) :
      monoid(m), hash(h), equal(e) {};
  static const key_type& get_key(in_type const &p) { return p.first;}
  static key_type& get_key(in_type &p) { return p.first;}
  void init(result_type &p, in_type const &kv) const {
    assign_uninitialized(p.second, kv.second);}
  void update(result_type &p, in_type const &kv) const {
    p.second = monoid.f(p.second, kv.second); }
  static void destruct_val(in_type &kv) {kv.second.~value_type();}
  template <typename Range>
  result_type reduce(Range &S) const {
    auto &key = S[0].first;
    auto sum = internal::reduce(delayed_map(S, [&] (in_type const &kv) {
      return kv.second;}), monoid);
    return result_type(key, sum);}
};

// Takes a range of <key_type,value_type> pairs and returns a sequence of
// the same type, but with equal keys combined into a single element.
// Values are combined with a monoid, which must be on the value type.
// Returned in an arbitrary order that depends on the hash function.
template <PARLAY_RANGE_TYPE R,
    typename Monoid = addm<typename range_value_type_t<R>::second_type>,
    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
auto reduce_by_key(R &&A, const Monoid& monoid = {}, Hash hash = {}, Equal equal = {}) {
  auto helper = reduce_by_key_helper<range_value_type_t<R>,Monoid,Hash,Equal>{monoid,hash,equal};
  return internal::collect_reduce_sparse(std::forward<R>(A), helper);
}

template <typename arg_type, typename Hash, typename Equal>
struct group_by_key_helper {
  using in_type = arg_type;
  using key_type = typename in_type::first_type;
  using val_type = typename in_type::second_type;
  using seq_type = sequence<val_type>;
  using result_type = std::pair<key_type,seq_type>;
  Hash hash; Equal equal;
  group_by_key_helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
  static const key_type& get_key(in_type const &p) { return p.first;}
  static key_type& get_key(in_type &p) { return p.first;}
  static key_type& get_key(result_type &p) {return p.first;}
  static void init(result_type &p, in_type const &kv) {
    assign_uninitialized(p.second,sequence(1, kv.second));}
  static void update(result_type &p, in_type const &kv) {
    p.second.push_back(kv.second); }
  static void destruct_val(in_type &kv) {kv.second.~val_type();}
  template <typename Range>
  static result_type reduce(Range &S) {
    auto &key = S[0].first;
    auto vals = map(S, [&] (in_type const &kv) {return kv.second;});
    return result_type(key, vals);}
};

template <PARLAY_RANGE_TYPE R,
    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
auto group_by_key(R &&A, Hash hash = {}, Equal equal = {}) {
  auto helper = group_by_key_helper<range_value_type_t<R>,Hash,Equal>{hash,equal};
  return internal::collect_reduce_sparse(std::forward<R>(A), helper);
}

template <typename arg_type, typename sum_type, typename Hash, typename Equal>
struct count_by_key_helper {
  using in_type = arg_type;
  using key_type = in_type;
  using val_type = sum_type;
  using result_type = std::pair<key_type,val_type>;
  Hash hash; Equal equal;
  count_by_key_helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
  static const key_type& get_key(in_type const &k) {return k;}
  static key_type& get_key(in_type &k) {return k;}
  static key_type& get_key(result_type &p) {return p.first;}
  static void init(result_type &p, in_type const&) {p.second = 1;}
  static void update(result_type &p, in_type const&) {p.second += 1;}
  static void destruct_val(in_type &) {}
  template <typename Range>
  static result_type reduce(Range &S) {return result_type(S[0], S.size());}
};

// Returns a sequence of <R::value_type,sum_type> pairs, each consisting of
// a unique value from the input, and the number of times it appears.
// Returned in an arbitrary order that depends on the hash function.
template <typename sum_type = size_t,
    PARLAY_RANGE_TYPE R,
    typename Hash = std::hash<range_value_type_t<R>>,
    typename Equal = std::equal_to<range_value_type_t<R>>>
auto histogram_by_key(R &&A, Hash hash = {}, Equal equal = {}) {
  auto helper = count_by_key_helper<range_value_type_t<R>,sum_type,Hash,Equal>{hash,equal};
  return internal::collect_reduce_sparse(std::forward<R>(A), helper);
}

template <typename arg_type, typename Hash, typename Equal>
struct remove_duplicates_helper {
  using in_type = arg_type;
  using key_type = in_type;
  using result_type = in_type;
  Hash hash; Equal equal;
  remove_duplicates_helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
  static const key_type& get_key(in_type const &k) { return k;}
  static key_type& get_key(in_type &k) { return k;}
  static void init(result_type &, in_type const&) {}
  static void update(result_type &, in_type const&) {}
  static void destruct_val(in_type &) {}
  template <typename Range>
  static result_type reduce(Range S) {return S[0];};
};

// should be made more efficient by avoiding generating and then stripping counts
template <PARLAY_RANGE_TYPE R,
    typename Hash = std::hash<range_value_type_t<R>>,
    typename Equal = std::equal_to<range_value_type_t<R>>>
auto remove_duplicates(R&& A, Hash hash = {}, Equal equal = {}) {
  auto helper = remove_duplicates_helper<range_value_type_t<R>,Hash,Equal>{hash,equal};
  return internal::collect_reduce_sparse(std::forward<R>(A), helper);
}

// Takes a range of <integer_key,value_type> pairs and returns a sequence of
// value_type, with all values corresponding to key i, combined at location i.
// Values are combined with a monoid, which must be on the value type.
// Must specify the number of buckets and it is an error for a key to be
// be out of range.
template <PARLAY_RANGE_TYPE R,
    typename Monoid = addm<typename range_value_type_t<R>::second_type>,
    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
auto reduce_by_index(R const &A, size_t num_buckets, const Monoid& monoid = {}) {
  struct helper {
    using in_type = range_value_type_t<R>;
    using key_type = typename in_type::first_type;
    using val_type = typename in_type::second_type;
    Monoid mon;
    static key_type get_key(const in_type& a) {return a.first;};
    static val_type get_val(const in_type& a) {return a.second;};
    val_type init() const {return mon.identity;}
    void update(val_type& d, val_type const &a) const {d = mon.f(d, a);};
    void combine(val_type& d, slice<in_type*,in_type*> s) const {
      auto vals = delayed_map(s, [&] (in_type &v) {return v.second;});
      d = internal::reduce(vals, mon);
    }
  };
  return internal::collect_reduce(make_slice(A), helper{monoid}, num_buckets);
}

// Given a sequence of integers creates a histogram with the count
// of each interger value. The range num_buckets must be specified
// and it is an error if any integers is out of the range [0:num_buckets).
template <typename Integer_t, PARLAY_RANGE_TYPE R>
auto histogram_by_index(R const &A, Integer_t num_buckets) {
  struct helper {
    using key_type = range_value_type_t<R>;
    using val_type = Integer_t;
    size_t n;
    static key_type get_key(const key_type& a) { return a;}
    static val_type get_val(const key_type&) { return 1;}
    static val_type init() {return 0;}
    void update(val_type& d, val_type a) const {d += a;}
    static void combine(val_type& d, slice<key_type*,key_type*> s) {
      d = s.size();}
  };
  return internal::collect_reduce(A, helper{A.size()}, num_buckets);
}

template <typename Integer_t, PARLAY_RANGE_TYPE R>
auto remove_duplicate_integers(R const &A, Integer_t max_value) {
  struct helper {
    using key_type = range_value_type_t<R>;
    using val_type = bool;
    static key_type get_key(const key_type& a) {return a;}
    static val_type get_val(const key_type&) {return true;}
    static val_type init() {return false;}
    static void update(val_type& d, val_type) {d = true;}
    static void combine(val_type& d, slice<key_type*,key_type*>) {
      d = true;}
  };
  auto flags = internal::collect_reduce(A, helper(), max_value);
  return pack(iota<Integer_t>(max_value), flags);
}

template <typename Integer_t, PARLAY_RANGE_TYPE R>
auto group_by_index(R const &A, Integer_t num_buckets) {
  if (A.size() > static_cast<size_t>(num_buckets)*num_buckets) {
    auto keys = delayed_map(A, [] (auto const &kv) {return kv.first;});
    auto vals = delayed_map(A, [] (auto const &kv) {return kv.second;});
    return internal::group_by_small_int(make_slice(vals), make_slice(keys), num_buckets);
  } else {
    struct helper {
      using in_type = range_value_type_t<R>;
      using key_type = typename in_type::first_type;
      using val_type = typename in_type::second_type;
      using result_type = sequence<val_type>;
      static key_type get_key(const in_type& a) {return a.first;}
      static val_type get_val(const in_type& a) {return a.second;}
      static result_type init() {return result_type();}
      static void update(result_type& d, const val_type& v) {d.push_back(v);}
      static void update(result_type& d, const result_type& v) {
        abort(); d.append(std::move(v));}
      static void combine(result_type& d, slice<in_type*,in_type*> s) {
        d =  map(s, [&] (in_type const &kv) {return get_val(kv);});}
    };
    return internal::collect_reduce(A, helper(), num_buckets);
  }
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace parlay

#endif  // PARLAY_INTERNAL_GROUP_BY_H_
