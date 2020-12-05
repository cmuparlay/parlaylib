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
  using uint = unsigned int;
  
// the following parameters can be tuned
constexpr const size_t CR_SEQ_THRESHOLD = 8192;

template <typename Seq, typename OutSeq, class Helper>
void seq_collect_reduce_few(Seq const &A, OutSeq &&Out, 
			    Helper const &helper, size_t num_buckets) {
  size_t n = A.size();
  for (size_t i = 0; i < num_buckets; i++)
    assign_uninitialized(Out[i], helper.init());
  for (size_t j = 0; j < n; j++) {
    size_t k = helper.get_key(A[j]);
    helper.update(Out[k], helper.get_val(A[j]));
  }
}

template <typename Seq, class Helper>
auto seq_collect_reduce_few(Seq const &A, Helper const &helper, size_t num_buckets) {
  using val_type = typename Helper::val_type;
  sequence<val_type> Out = sequence<val_type>::uninitialized(num_buckets);
  seq_collect_reduce_few(A, Out, helper, num_buckets);
  return Out;
}

// This one is for few buckets (e.g. less than 2^16)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
//  all keys must be smaller than num_buckets
template <typename Seq, typename Helper>
auto collect_reduce_few(Seq const &A, Helper const &helper, size_t num_buckets) {
  using val_type = typename Helper::val_type;
  size_t n = A.size();

  // pad to 16 buckets to avoid false sharing (does not affect results)
  num_buckets = (std::max)(num_buckets, (size_t)16);

  // size_t num_blocks = ceil(pow(n/num_buckets,0.5));
  size_t num_threads = num_workers();
  size_t num_blocks = (std::min)(4 * num_threads, n / num_buckets / 64);

  num_blocks = size_t{1} << log2_up(num_blocks);

  sequence<val_type> Out(num_buckets);

  // if insufficient parallelism, do sequentially
  if (n < CR_SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1)
    return seq_collect_reduce_few(A, helper, num_buckets);

  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  sequence<val_type> OutM = sequence<val_type>::uninitialized(m);

  sliced_for(n, block_size, [&](size_t i, size_t start, size_t end) {
    seq_collect_reduce_few(make_slice(A).cut(start, end),
                           make_slice(OutM).cut(i * num_buckets, (i + 1) * num_buckets),
                           helper, num_buckets);
  });
  
  parallel_for(0, num_buckets, [&](size_t i) {
       val_type o_val = helper.init();
       for (size_t j = 0; j < num_blocks; j++)
	 helper.update(o_val, OutM[i + j * num_buckets]);
       Out[i] = o_val; }, 1);
  return Out;
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
  uint table_mask;
  uint bucket_mask;
  uint heavy_hitters;
  const HashEq hasheq;

  // creates a structure from a sequence of elements
  // bits is the number of bits that will be returned by the hash function
  // items that appear many times will be mapped individually into the earliest buckets
  // heavy_hitters will indicate how many of these there are
  template <typename Seq>
  get_bucket(Seq const &A, HashEq const &hasheq, size_t bits) : hasheq(hasheq) {
    size_t n = A.size();
    size_t num_buckets = size_t{1} << bits; 
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

    // add to the hash table if at least four copies and give added items
    // consecutive numbers.   
    heavy_hitters = 0;
    hash_table = sequence<KI>(table_size, std::make_pair(key_type(), -1));
    for (size_t i = 0; i < table_size; i++) {
      if (hash_table_count[i].second > 2) {
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
    uint hash_val = hasheq.hash(hasheq.get_key(v));
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
  using T = typename Seq::value_type;
  size_t n = A.size();
  //using key_type = typename Helper::key_type;
  using val_type = typename Helper::val_type;

  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = std::max<size_t>(
      log2_up(1 + 2 * (size_t) sizeof(T) * n / cache_per_thread), 4);
  size_t num_blocks = size_t{1} << bits;
  
  if (num_buckets <= 4 * num_blocks)
    return collect_reduce_few(A, helper, num_buckets);

  // allocate results.  Shift is to align cache lines.
  sequence<val_type> sums_s(num_buckets, helper.init());
  auto sums = sums_s.begin();
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
  
  // now process each block in parallel
  parallel_for(0, num_blocks, [&] (size_t i) {
    auto slice = B.cut(block_offsets[i], block_offsets[i + 1]);

    if (i < gb.heavy_hitters) // heavy hitters with all equal keys
      helper.combine(sums[helper.get_key(slice[0])], slice);
    else { // shared blocks
      for (size_t j = 0; j < slice.size(); j++) {
	size_t k = helper.get_key(slice[j]);
	helper.update(sums[k], helper.get_val(slice[j]));
      }
    }
  }, 1);
  return sums_s;
}

  template <typename Slice, typename Helper>
  auto seq_collect_reduce_sparse(Slice A, Helper const &helper) {
    size_t table_size = 1.5 * A.size();
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
      if (flags[k]) helper.update(table[k], A[j]);
      else {
	flags[k] = true;
	count++;
	assign_uninitialized(table[k], helper.make(A[j]));
      }
    }

    // pack non-empty entries of table into result sequence
    // ideally this would relocate instead of a move
    auto r_s = sequence<result_type>::uninitialized(count); 
    auto r = r_s.begin();
    size_t j = 0;
    for (size_t i = 0; i < table_size; i++) 
      if (flags[i]) move_uninitialized(r[j++], table[i]);
    assert(j == count);
    return r_s;
  }

// this one is for more buckets than the length of A (i.e. sparse)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
template <typename Slice, typename Helper>
auto collect_reduce_sparse(Slice A, Helper const &helper) {
  timer t("collect reduce sparse", false);
  using in_type = typename Helper::in_type;
  //using key_type = typename Helper::key_type;
  size_t n = A.size();

  if (n < 10000) return seq_collect_reduce_sparse(A, helper);
  
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
    
  // first buckets based on hash using an integer sort
  // Guy : could possibly just use counting sort.  Would avoid needing tmp
  auto B = sequence<in_type>::uninitialized(n);
  uninitialized_sequence<in_type> Tmp(n); 
  sequence<size_t> bucket_offsets =
    integer_sort_<std::false_type, uninitialized_copy_tag>(
      make_slice(A), make_slice(B), make_slice(Tmp), gb, bits, num_buckets);
  t.next("integer sort");

  uint heavy_cutoff = gb.heavy_hitters;

  // now in parallel process each bucket sequentially, returning a packed sequence
  // of the results within that bucket
  auto tables = tabulate(num_buckets, [&] (size_t i) {
      auto block = B.cut(bucket_offsets[i], bucket_offsets[i+1]);				
      if (i < heavy_cutoff) return sequence(1,helper.reduce(block));
      else return seq_collect_reduce_sparse(block, helper);
    }, 1);
  t.next("block hash");

  // flatten the results
  return flatten(std::move(tables));
}

} // namespace internal

  // ***************************************
  // User facing interface below here
  // ***************************************
  
  // Takes a range of <key_type,value_type> pairs and returns a sequence of
  // the same type, but with equal keys combined into a single element.
  // Values are combined with a monoid, which must be on the value type.
  // Returned in an arbitrary order that depends on the hash function.
  template <PARLAY_RANGE_TYPE R,
  	    typename Monoid,
  	    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
  	    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
  auto reduce_by_key(R const &A, Monoid const &monoid, Hash hash = {}, Equal equal = {}) { 
    struct helper {
      using in_type = range_value_type_t<R>;
      using key_type = typename in_type::first_type;
      using result_type = in_type;
      Monoid monoid; Hash hash; Equal equal;
      helper(Monoid const &m, Hash const &h, Equal const &e) : 
	monoid(m), hash(h), equal(e) {};
      static result_type make(in_type const &kv) { return kv;}
      static const key_type& get_key(in_type const &p) { return p.first;}
      void update(result_type &p, in_type const &v) const {
	p.second = monoid.f(p.second, v.second); }
      result_type reduce(slice<in_type*,in_type*> &S) const {
	auto &key = S[0].first;
	auto sum = internal::reduce(delayed_map(S, [&] (in_type const &kv) {
			return kv.second;}), monoid);
	return result_type(key, sum);}
    };
    return internal::collect_reduce_sparse(make_slice(A), helper{monoid, hash, equal});
  }

  template <PARLAY_RANGE_TYPE R,
  	    typename Hash = std::hash<typename range_value_type_t<R>::first_type>,
  	    typename Equal = std::equal_to<typename range_value_type_t<R>::first_type>>
  auto group_by_key(R const &A, Hash hash = {}, Equal equal = {}) { 
    struct helper {
      using in_type = range_value_type_t<R>;
      using key_type = typename in_type::first_type;
      using val_type = typename in_type::second_type;
      using seq_type = sequence<val_type>;
      using result_type = std::pair<key_type,seq_type>;
      Hash hash; Equal equal;
      helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
      static result_type make(in_type const &kv) {
	return std::pair(kv.first,sequence(1,kv.second));}
      static const key_type& get_key(in_type const &p) { return p.first;}
      static const key_type& get_key(result_type const &p) {return p.first;}
      void update(result_type &p, in_type const &v) const {
	p.second.push_back(v.second); }
      result_type reduce(slice<in_type*,in_type*> &S) const {
	auto &key = S[0].first;
	auto vals = map(S, [&] (in_type const &kv) {return kv.second;});
	return result_type(key, vals);}
    };
    return internal::collect_reduce_sparse(make_slice(A), helper{hash, equal});
  }

  // Returns a sequence of <R::value_type,sum_type> pairs, each consisting of
  // a unique value from the input, and the number of times it appears.
  // Returned in an arbitrary order that depends on the hash function.
  template <typename sum_type,
	    PARLAY_RANGE_TYPE R,
	    typename Hash = std::hash<range_value_type_t<R>>,
	    typename Equal = std::equal_to<range_value_type_t<R>>>
  sequence<std::pair<range_value_type_t<R>,sum_type>>
  count_by_key(R const &A, Hash hash = {}, Equal equal = {}) { 
    struct helper {
      using in_type = range_value_type_t<R>;
      using key_type = in_type;
      using val_type = sum_type;
      using result_type = std::pair<key_type,val_type>;
      Hash hash; Equal equal;
      helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
      static inline result_type make(in_type const &k) {
	return result_type(k,1);}
      static const key_type& get_key(in_type const &k) {return k;}
      static const key_type& get_key(result_type const &p) {return p.first;}
      static void update(result_type &p, in_type const&) {p.second += 1;}
      static result_type reduce(slice<in_type*,in_type*> &S) {
	return result_type(S[0], S.size());}
    };
    return internal::collect_reduce_sparse(make_slice(A), helper{hash,equal});
  }

  template <PARLAY_RANGE_TYPE R,
	    typename Hash = std::hash<range_value_type_t<R>>,
	    typename Equal = std::equal_to<range_value_type_t<R>>>
  auto count_by_key(R const &A, Hash hash = {}, Equal equal = {}) { 
    return count_by_key<size_t>(A, hash, equal);}

  // should be made more efficient by avoiding generating and then stripping counts
  template <PARLAY_RANGE_TYPE R,
	    typename Hash = std::hash<range_value_type_t<R>>,
	    typename Equal = std::equal_to<range_value_type_t<R>>>
  auto remove_duplicates(const R& A, Hash hash = {}, Equal equal = {}) { 
    struct helper {
      using in_type = range_value_type_t<R>;
      using key_type = in_type;
      using result_type = in_type;
      Hash hash; Equal equal;
      helper(Hash const &h, Equal const &e) : hash(h), equal(e) {};
      static inline result_type make(in_type const &k) {return k;}
      static const key_type& get_key(in_type const &k) { return k;}
      static void update(result_type &, in_type const&) {}
      static result_type reduce(slice<in_type*,in_type*> S) {return S[0];};
    };
    return internal::collect_reduce_sparse(make_slice(A), helper{hash, equal});
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
      using pair_type = range_value_type_t<R>;
      using key_type = typename pair_type::first_type;
      using val_type = typename pair_type::second_type;
      Monoid mon;
      static key_type get_key(const pair_type& a) {return a.first;};
      static val_type get_val(const pair_type& a) {return a.second;};
      val_type init() const {return mon.identity;}
      void update(val_type& d, val_type const &a) const {d = mon.f(d, a);};
      void combine(val_type& d, slice<pair_type*,pair_type*> s) const {
	auto vals = delayed_map(s, [&] (pair_type &v) {return v.second;});
	d = internal::reduce(vals, mon);
      }
    };
    return internal::collect_reduce(make_slice(A), helper{monoid}, num_buckets);
  }

  // Given a sequence of integers creates a histogram with the count
  // of each interger value.   The range num_buckets must be specified
  // and it is an error if any integers is out of the range [0:num_buckets).
  template <typename Integer_t, PARLAY_RANGE_TYPE R>
  auto histogram(R const &A, Integer_t num_buckets) {
    struct helper {
      using key_type = range_value_type_t<R>;
      using val_type = Integer_t;
      static key_type get_key(const key_type& a) { return a;}
      static val_type get_val(const key_type&) { return 1;}
      static val_type init() {return 0;}
      static void update(val_type& d, val_type a) {d += a;}
      static void combine(val_type& d, slice<key_type*,key_type*> s) {
	d = s.size();}
    };
    return internal::collect_reduce(A, helper(), num_buckets);
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

}  // namespace parlay

#endif  // PARLAY_COLLECT_REDUCE_H_
