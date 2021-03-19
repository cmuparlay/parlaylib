#ifndef PARLAY_COLLECT_REDUCE_H_
#define PARLAY_COLLECT_REDUCE_H_

#include <cmath>
#include <cstdio>

#include "integer_sort.h"
#include "uninitialized_sequence.h"
#include "get_time.h"

#include "../utilities.h"
#include "../primitives.h"
#include "../delayed_sequence.h"

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t CR_SEQ_THRESHOLD = 8192;

// Sequential version of collect_reduce
// The helper must supply an init() function to initialize a bucket,
// a get_key(.) function to extrat a key from an element of A, 
// a get_val(.) function to extract a value from an element of A, and
// a update(.,.) function to add in a new value into a bucket.
// all keys must be less than num_buckets.
template <typename Seq, class Helper>
auto seq_collect_reduce(Seq const &A, Helper const &helper, size_t num_buckets) {
  using result_type = decltype(helper.init());
  sequence<result_type> Out(num_buckets, helper.init());
  for (size_t j = 0; j < A.size(); j++) {
    size_t k = helper.get_key(A[j]);
    assert(k < num_buckets);
    helper.update(Out[k], helper.get_val(A[j]));
  }
  return Out;
}

// This is for few buckets (e.g. less than 2^16)
template <typename Seq, typename Helper>
auto collect_reduce_few(Seq const &A, Helper const &helper, size_t num_buckets) {
  using result_type = decltype(helper.init());
  size_t n = A.size();

  size_t num_threads = num_workers();
  size_t num_blocks = (std::min)(4 * num_threads, n / num_buckets / 64) + 1;

  // if insufficient parallelism, do sequentially
  if (n < CR_SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1)
    return seq_collect_reduce(A, helper, num_buckets);

  // partial results for each block
  size_t block_size = ((n - 1) / num_blocks) + 1;
  sequence<sequence<result_type>> Out(num_blocks);
  sliced_for(n, block_size, [&](size_t i, size_t start, size_t end) {
    Out[i] = seq_collect_reduce(make_slice(A).cut(start, end),
                                helper, num_buckets);
  });

  // comibine partial results into total result
  return tabulate(num_buckets, [&](size_t i) {
    result_type o_val = helper.init();
    for (size_t j = 0; j < num_blocks; j++)
      helper.update(o_val, Out[j][i]);
    return o_val; }, 1);
}

// The idea is to return a hash function that maps any items
// that appear many times into their own bucket.
// Otherwise items can end up in the same bucket.
// E is the type of element
// HashEq must contain 
//    a key_type
//    a hash function : key_type -> size_t
//    an equality function : key_type x key_type -> bool
template <typename HashEq>
struct get_bucket {
  using in_type = typename HashEq::in_type;
  using key_type = typename HashEq::key_type;
  using KI = std::pair<key_type, int>;
  sequence<KI> hash_table;
  size_t table_mask;
  size_t bucket_mask;
  size_t heavy_hitters;
  const HashEq hasheq;

  // creates a structure from a sequence of elements
  // bits is the number of bits that will be returned by the hash function
  // items that appear many times will be mapped individually into the earliest buckets
  // heavy_hitters will indicate how many of these there are
  template <typename Seq>
  get_bucket(Seq const &A, HashEq const &hasheq, size_t bits) : hasheq(hasheq) {
    size_t n = A.size();
    size_t num_buckets = size_t{1} << bits;
    int copy_cutoff = 5; // number of copies in sample to be considered a heavy hitter
    size_t num_samples = num_buckets;
    size_t table_size = 4 * num_samples;
    table_mask = table_size - 1;
    bucket_mask = num_buckets - 1;

    auto hash_table_count = sequence<KI>(table_size, std::make_pair(key_type(), -1));

    // insert sample into hash table with one less than the
    // count of how many times appears (since it starts with -1)
    for (size_t i = 0; i < num_samples; i++) {
      const key_type& s = hasheq.get_key(A[hash64(i) % n]);
      size_t idx = hasheq.hash(s) & table_mask;
      while (1) {
        if (hash_table_count[idx].second == -1) {
          hash_table_count[idx] = std::make_pair(s, 0);
          break;
        } else if (hasheq.equal(hash_table_count[idx].first, s)) {
          hash_table_count[idx].second += 1;
          break;
        } else
          idx = (idx + 1) & table_mask;
      }
    }

    // add to the hash table if at least copy_cutoff copies appear in sample
    // give added items consecutive numbers.   
    heavy_hitters = 0;
    hash_table = sequence<KI>(table_size, std::make_pair(key_type(), -1));
    for (size_t i = 0; i < table_size; i++) {
      if (hash_table_count[i].second + 2 > copy_cutoff) {
        key_type key = hash_table_count[i].first;
        size_t idx = hasheq.hash(key) & table_mask;
        if (hash_table[idx].second == -1)
          hash_table[idx] = std::make_pair(key, heavy_hitters++);
      }
    }
  }

  // the hash function.
  // uses chosen bucket from preprocessing if key appears many times (heavy)
  // otherwise uses one of the remaining buckets
  size_t operator()(in_type const &v) const {
    //key_type& key = ;
    auto hash_val = hasheq.hash(hasheq.get_key(v));
    if (heavy_hitters > 0) {
      auto &h = hash_table[hash_val & table_mask];
      if (h.second != -1 && hasheq.equal(h.first, hasheq.get_key(v))) return h.second;
      hash_val = hash_val & bucket_mask;
      if ((hash_val & bucket_mask) < heavy_hitters)
        return hash_val % (bucket_mask+1-heavy_hitters) + heavy_hitters;
      return hash_val & bucket_mask;
    }
    return hash_val & bucket_mask;
  }
};

template <typename Seq, class Helper>
auto collect_reduce(Seq const &A, Helper const &helper, size_t num_buckets) {
  timer t("collect reduce", false);
  using T = typename Seq::value_type;
  size_t n = A.size();
  //using val_type = typename Helper::val_type;
  using result_type = decltype(helper.init());

  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = std::max<size_t>(
      log2_up(1 + (2 * (size_t) sizeof(T) * n) / cache_per_thread), 4);
  size_t num_blocks = size_t{1} << bits;

  if (num_buckets <= 4 * num_blocks || n < CR_SEQ_THRESHOLD)
    return collect_reduce_few(A, helper, num_buckets);

  // Shift is to align cache lines.
  constexpr size_t shift = 8 / sizeof(T);

  // Returns a map (hash) from key to block.
  // Keys with many elements (big) have their own block while
  // others share a block.
  // Keys that share low 4 bits get same block unless big.
  // This is to avoid false sharing.
  struct hasheq {
    Helper helper;
    using in_type = T;
    using key_type = typename Helper::key_type;
    static size_t hash(key_type const &a) {
      return hash64_2((a+shift) & ~((size_t)15)); }
    static bool equal(key_type const& a, key_type const& b) { return a == b; }
    key_type get_key(const in_type &v) const {return helper.get_key(v);}
  };
  get_bucket<hasheq> gb(A, hasheq{helper}, bits);

  sequence<T> B = sequence<T>::uninitialized(n);
  sequence<T> Tmp = sequence<T>::uninitialized(n);

  // first partition into blocks based on hash using a counting sort
  sequence<size_t> block_offsets;
  block_offsets = integer_sort_<std::false_type, uninitialized_copy_tag>(
      make_slice(A), make_slice(B), make_slice(Tmp), gb, bits, num_blocks);
  t.next("sort");

  // results
  sequence<result_type> sums_s(num_buckets, helper.init());
  auto sums = sums_s.begin();

  // now process each block in parallel
  parallel_for(0, num_blocks, [&] (size_t i) {
    auto slice = B.cut(block_offsets[i], block_offsets[i + 1]);

    if (i < gb.heavy_hitters) // heavy hitters with all equal keys
      helper.combine(sums[helper.get_key(slice[0])], slice);
    else { // shared blocks
      for (size_t j = 0; j < slice.size(); j++) {
        size_t k = helper.get_key(slice[j]);
        assert(k < num_buckets);
        helper.update(sums[k], helper.get_val(slice[j]));
      }
    }
  });
  t.next("into tables");
  return sums_s;
}

template <typename assignment_tag, typename Slice, typename Helper>
auto seq_collect_reduce_sparse(Slice A, Helper const &helper) {
  size_t table_size = 1.5 * A.size();
  //using in_type = typename Helper::in_type;
  using key_type = typename Helper::key_type;
  using result_type = typename Helper::result_type;

  size_t count=0;
  uninitialized_sequence<result_type> table_s (table_size);
  sequence<bool> flags_s(table_size, false);
  auto table = table_s.begin();
  auto flags = flags_s.begin();

  // hash into buckets
  for (size_t j = 0; j < A.size(); j++) {
    key_type const &key = helper.get_key(A[j]);
    size_t k = ((size_t) helper.hash(key)) % table_size;
    while (flags[k] && !helper.equal(helper.get_key(table[k]), key))
      k = (k + 1 == table_size) ? 0 : k + 1;
    if (flags[k]) {
      helper.update(table[k], A[j]);
      // if relocating input need to destruct key
      if constexpr (std::is_same<assignment_tag,uninitialized_relocate_tag>::value)
        helper.get_key(A[j]).~key_type();
    } else {
      flags[k] = true;
      count++;
      helper.init(table[k], A[j]);
      assign_dispatch(helper.get_key(table[k]),
                      helper.get_key(A[j]),
                      assignment_tag());
    }
    // if relocating input need to destruct value
    if constexpr (std::is_same<assignment_tag,uninitialized_relocate_tag>::value)
      helper.destruct_val(A[j]);
  }

  // pack non-empty entries of table into result sequence
  auto r_s = sequence<result_type>::uninitialized(count);
  auto r = r_s.begin();
  size_t j = 0;
  for (size_t i = 0; i < table_size; i++)
    if (flags[i]) uninitialized_relocate(&r[j++], &table[i]);
  assert(j == count);
  return r_s;
}

// this one is for more buckets than the length of A (i.e. sparse)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
template <typename assignment_tag, typename Slice, typename Helper>
auto collect_reduce_sparse_(Slice A, Helper const &helper) {
  timer t("collect reduce sparse", false);
  using in_type = std::remove_const_t<typename Helper::in_type>;
  size_t n = A.size();

  if (n < 10000) return seq_collect_reduce_sparse<assignment_tag>(A, helper);

  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = log2_up(
      (size_t)(1 + (1.2 * 2 * sizeof(in_type) * n) / (float)cache_per_thread));
  bits = std::max<size_t>(bits, 4);
  size_t num_buckets = (1 << bits);

  // Returns a map (hash) from key to bucket.
  // Keys with many elements (heavy) have their own bucket while
  // others share a bucket.
  auto gb = get_bucket<Helper>(A, helper, bits);

  // first bucket based on hash using an integer sort
  uninitialized_sequence<in_type> B(n);
  auto keys = delayed_tabulate(n, [&] (size_t i) {return gb(A[i]);});
  auto bucket_offsets =
      count_sort<assignment_tag>(make_slice(A), make_slice(B),
                                 make_slice(keys), num_buckets).first;
  t.next("integer sort");
  // all buckets up to heavy_cutoff have a single key in them
  size_t heavy_cutoff = gb.heavy_hitters;

  // now in parallel process each bucket sequentially, returning a sequence
  // of the results within that bucket
  // Note that all elements of B need to be destructed within this code
  // since no destructor will be called on B after it.
  auto tables = tabulate(num_buckets, [&] (size_t i) {
    auto block = make_slice(B).cut(bucket_offsets[i], bucket_offsets[i+1]);
    if (i < heavy_cutoff) {
      auto a = sequence(1,helper.reduce(block));
      if constexpr (!std::is_trivially_destructible_v<in_type>)
        parallel_for(0, block.size(), [&] (size_t i) {block[i].~in_type();}, 1000);
      return a;
    } else return seq_collect_reduce_sparse<uninitialized_relocate_tag>(block, helper);
  }, 1);
  t.next("block hash");

  // flatten the results
  return flatten(std::move(tables));
}

template <typename T, typename Helper>
auto collect_reduce_sparse(sequence<T> &&A, Helper const &helper) {
  auto r = collect_reduce_sparse_<uninitialized_relocate_tag>(make_slice(A), helper);
  clear_relocated(A);
  return r;
}

template <typename Range, typename Helper>
auto collect_reduce_sparse(Range const &A, Helper const &helper) {
  return collect_reduce_sparse_<uninitialized_copy_tag>(make_slice(A), helper);
}

} // namespace internal

// ***************************************
// User facing interface below here
// ***************************************

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
    typename Monoid,
    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
auto reduce_by_key(R &&A, Monoid const &monoid, Hash hash = {}, Equal equal = {}) {
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
template <typename sum_type,
    PARLAY_RANGE_TYPE R,
    typename Hash = std::hash<range_value_type_t<R>>,
typename Equal = std::equal_to<range_value_type_t<R>>>
sequence<std::pair<range_value_type_t<R>,sum_type>>
count_by_key(R &&A, Hash hash = {}, Equal equal = {}) {
auto helper = count_by_key_helper<range_value_type_t<R>,sum_type,Hash,Equal>{hash,equal};
return internal::collect_reduce_sparse(std::forward<R>(A), helper);
}

template <PARLAY_RANGE_TYPE R,
    typename Hash = std::hash<range_value_type_t<R>>,
typename Equal = std::equal_to<range_value_type_t<R>>>
auto count_by_key(R &&A, Hash hash = {}, Equal equal = {}) {
  return count_by_key<size_t>(std::forward<R>(A), hash, equal);}

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
    typename Monoid,
    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
auto reduce_by_index(R const &A, size_t num_buckets, Monoid const &monoid) {
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
auto histogram(R const &A, Integer_t num_buckets) {
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
auto remove_duplicates_by_index(R const &A, Integer_t num_buckets) {
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
  auto flags = internal::collect_reduce(A, helper(), num_buckets);
  return pack(iota<Integer_t>(num_buckets), flags);
}

template <typename Integer_t, PARLAY_RANGE_TYPE R>
auto group_by_index(R const &A, Integer_t num_buckets) {
  if (A.size() > num_buckets*num_buckets) {
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

}  // namespace parlay

#endif  // PARLAY_COLLECT_REDUCE_H_
