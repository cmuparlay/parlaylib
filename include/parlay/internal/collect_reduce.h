// Supports functions that take a seq of key-value pairs, collects all the
// keys with the same value, and sums them up with a binary function (monoid)
//
// For the first one the keys must be integers in the range [0,num_buckets).
// It returns a sequence of length num_buckets, one sum per possible key value.
//
//   template <typename Seq, typename M>
//   sequence<typename Seq::value_type::second_type>
//   collect_reduce(Seq const &A, M const &monoid, size_t num_buckets);
//
// For the second one keys can be any integer values
// It returns a sequence of key-value pairs.  If a key appeared at least
//   once, an entry with the sum for that key will appear in the output.
// The output is not necessarily sorted by key value
//
//   template <typename Seq, typename M>
//   sequence<typename Seq::value_type>
//   collect_reduce_sparse(Seq const &A, M const &monoid);

#ifndef PARLAY_COLLECT_REDUCE_H_
#define PARLAY_COLLECT_REDUCE_H_

#include <cmath>
#include <cstdio>

#include "integer_sort.h"
#include "uninitialized_sequence.h"
#include "sequence_ops.h"
#include "transpose.h"

#include "../utilities.h"
//#include "../../../pbbstimings/get_time.h"

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t CR_SEQ_THRESHOLD = 8192;

template <typename Seq, typename OutSeq, class Key, class Value, typename M>
void seq_collect_reduce_few(Seq const &A, OutSeq &&Out, Key const &get_key,
                            Value const &get_value, M const &monoid,
                            size_t num_buckets) {
  size_t n = A.size();
  for (size_t i = 0; i < num_buckets; i++)
    assign_uninitialized(Out[i],monoid.identity);
  for (size_t j = 0; j < n; j++) {
    size_t k = get_key(A[j]);
    Out[k] = monoid.f(Out[k], get_value(A[j]));
  }
}

template <typename Seq, class Key, class Value, typename M>
auto seq_collect_reduce_few(Seq const &A, Key const &get_key,
                            Value const &get_value, M const &monoid,
                            size_t num_buckets)
    -> sequence<decltype(get_value(A[0]))> {
  using val_type = decltype(get_value(A[0]));
  sequence<val_type> Out = sequence<val_type>::uninitialized(num_buckets);
  seq_collect_reduce_few(A, Out, get_key, get_value, monoid, num_buckets);
  return Out;
}

// This one is for few buckets (e.g. less than 2^16)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
//  all keys must be smaller than num_buckets
template <typename Seq, class Key, class Value, typename M>
auto collect_reduce_few(Seq const &A, Key const &get_key,
                        Value const &get_value, M const &monoid,
                        size_t num_buckets)
    -> sequence<decltype(get_value(A[0]))> {
  using val_type = decltype(get_value(A[0]));
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
    return seq_collect_reduce_few(A, get_key, get_value, monoid, num_buckets);

  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  sequence<val_type> OutM = sequence<val_type>::uninitialized(m);

  sliced_for(n, block_size, [&](size_t i, size_t start, size_t end) {
    seq_collect_reduce_few(make_slice(A).cut(start, end),
                           make_slice(OutM).cut(i * num_buckets, (i + 1) * num_buckets),
                           get_key, get_value, monoid, num_buckets);
  });
  
  parallel_for(0, num_buckets,
               [&](size_t i) {
                 val_type o_val = monoid.identity;
                 for (size_t j = 0; j < num_blocks; j++)
                   o_val = monoid.f(o_val, OutM[i + j * num_buckets]);
                 Out[i] = o_val;
               },
               1);
  return Out;
}

// The idea is to return a hash function that maps any items
// that appear many times into their own bucket.
// Otherwise items can end up in the same bucket.
// E is the type of element
// HashEq must contain an hash function E -> size_t
//    and an equality function E x E -> bool
template <typename E, typename K, typename HashEq, typename GetKey>
struct get_bucket {
  using key_type = K;
  using KI = std::pair<K, std::ptrdiff_t>;
  sequence<KI> hash_table;
  size_t table_mask;
  size_t bucket_mask;
  size_t num_buckets;
  bool heavy_hitters;
  const HashEq heq;
  const GetKey get_key;

  // creates a structure from a sequence of elements
  // bits is the number of bits that will be returned by the hash function
  // items that appear many times will be mapped individually into
  //    the top half [2^{bits-1},2^{bits})
  //    and light items shared into the bottom half [0,2^{bits-1})
  template <typename Seq>
  get_bucket(Seq const &A, HashEq const &heq, GetKey get_key, size_t bits)
    : heq(heq), get_key(get_key) {
    size_t n = A.size();
    size_t low_bits = bits - 1;   // for the bottom half
    num_buckets = size_t{1} << low_bits;  // in bottom half
    size_t count = 2 * num_buckets;
    size_t table_size = 4 * count;
    table_mask = table_size - 1;

    hash_table = sequence<KI>(table_size, std::make_pair(K(), -1));

    // insert sample into hash table with one less than the
    // count of how many times appears (since it starts with -1)
    for (size_t i = 0; i < count; i++) {
      K s = get_key(A[hash64(i) % n]);
      size_t idx = heq.hash(s) & table_mask;
      while (1) {
        if (hash_table[idx].second == -1) {
          hash_table[idx] = std::make_pair(s, 0);
          break;
        } else if (heq.equal(hash_table[idx].first, s)) {
          hash_table[idx].second += 1;
          break;
        } else
          idx = (idx + 1) & table_mask;
      }
    }

    // keep in the hash table if at least three copies and give kept items
    // consecutive numbers.   k will be total kept items.
    size_t k = 0;
    for (size_t i = 0; i < table_size; i++) {
      if (hash_table[i].second > 1) {
        K key = hash_table[i].first;
        size_t idx = heq.hash(key) & table_mask;
        hash_table[idx] = std::make_pair(key, k++);
      } else
        hash_table[i].second = -1;
    }

    heavy_hitters = (k > 0);
    bucket_mask = heavy_hitters ? num_buckets - 1 : 2 * num_buckets - 1;
  }

  // the hash function.
  // uses chosen id if key appears many times (top half)
  // otherwise uses (heq.hash(v) % num_buckets) directly (bottom half)
  size_t operator()(E const &v) const {
    const K& key = get_key(v);
    size_t hash_val = heq.hash(key);
    if (heavy_hitters) {
      auto &h = hash_table[hash_val & table_mask];
      if (h.second != -1 && heq.equal(h.first, key))
        return h.second + num_buckets;  // top half
    }
    return hash_val & bucket_mask;  // bottom half
  }
};

template <typename K, int offset>
struct hasheq_mask_low {
  inline size_t hash(K a) const { return hash64_2((a+offset) & ~((size_t)15)); }
  inline bool equal(K a, K b) const { return a == b; }
};

template <typename Seq, class Key, class Value, typename M>
auto collect_reduce(Seq const &A, Key const &get_key, Value const &get_value,
                    M const &monoid, size_t num_buckets) {
  //-> sequence<decltype(get_value(A[0]))> {
  //timer t("collect reduce");
  using T = typename Seq::value_type;
  using key_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_key(A[0]))>>;
  using val_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_value(A[0]))>>;
  size_t n = A.size();

  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = std::max<size_t>(
      log2_up(1 + 2 * (size_t)sizeof(val_type) * n / cache_per_thread), 4);

  size_t num_blocks = size_t{1} << bits;
  
  if (num_buckets <= 4 * num_blocks)
    return collect_reduce_few(A, get_key, get_value, monoid, num_buckets);

  // allocate results.  Shift is to align cache lines.
  sequence<val_type> sums(num_buckets, monoid.identity);
  constexpr size_t shift = 8 / sizeof(T);

  // Returns a map (hash) from key to block.
  // Keys with many elements (big) have their own block while
  // others share a block.
  // Keys that share low 4 bits get same block unless big.
  // This is to avoid false sharing.
  // auto get_i = [&] (size_t i) -> size_t {return A[i].first;};
  // auto s = delayed_seq<size_t>(n,get_i);
  
  using hasheq = hasheq_mask_low<key_type,shift>;
  get_bucket<T, key_type, hasheq, Key> gb(A, hasheq(), get_key, bits);
  sequence<T> B = sequence<T>::uninitialized(n);
  sequence<T> Tmp = sequence<T>::uninitialized(n);
  
  // first partition into blocks based on hash using a counting sort
  sequence<size_t> block_offsets;
  block_offsets = integer_sort_<std::false_type, uninitialized_copy_tag>(
    make_slice(A), make_slice(B), make_slice(Tmp) , gb, bits, num_blocks);
  
  // now process each block in parallel
  parallel_for(0, num_blocks,
               [&](size_t i) {
                 size_t start = block_offsets[i];
                 size_t end = block_offsets[i + 1];
                 size_t cut = gb.heavy_hitters ? num_blocks / 2 : num_blocks;

                 // small blocks have indices in bottom half
                 if (i < cut)
                   for (size_t i = start; i < end; i++) {
                     size_t j = get_key(B[i]);
                     sums[j] = monoid.f(sums[j], get_value(B[i]));
                   }

                 // large blocks have indices in top half
                 else if (end > start) {
                   auto x = [&](size_t i) -> val_type {
                     return get_value(B[i]);
                   };
                   auto vals = delayed_seq<val_type>(n, x);
                   sums[get_key(B[i])] = internal::reduce(vals, monoid);
                 }
               },
               1);
  return sums;
}


  template <typename Iterator, typename HashEq, typename GetK, typename GetV, typename M>
  auto seq_collect_reduce_sparse(slice<Iterator,Iterator> A, slice<Iterator,Iterator> Heavy,
				 HashEq hasheq, GetK get_key, GetV get_val,
				 M const &monoid) {
    size_t table_size = 1.5 * A.size();
    //using T = typename slice<Iterator, Iterator>::value_type;
    using key_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_key(A[0]))>>;
    using val_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_val(A[0]))>>;
    using result_type = std::pair<key_type,val_type>;

    size_t count=0;
    uninitialized_sequence<result_type> table (table_size);
    sequence<bool> flags(table_size, false);
		 
    // insert small buckets (ones with multiple different items)
    for (size_t j = 0; j < A.size(); j++) {
      key_type const &key = get_key(A[j]);
      size_t k = ((size_t) hasheq.hash(key)) % table_size;
      while (flags[k] && !hasheq.equal(table[k].first, key))
	k = (k + 1 == table_size) ? 0 : k + 1;
      if (flags[k]) {
	table[k].second = monoid.f(table[k].second, get_val(A[j]));
      } else {
	flags[k] = true;
	count++;
	assign_uninitialized(table[k], result_type(key, get_val(A[j])));
      }
    }

    // now if there is a "heavy hitter" (bucket with a single key)
    // insert it using reduce to sum the values in parallel
    if (Heavy.size() > 0) {
      auto f = [&] (size_t i) -> val_type {
	return get_val(Heavy[i]);
      };
      auto s = delayed_seq<val_type>(Heavy.size(), f);
      val_type val = internal::reduce(s, monoid);
      size_t j = 0;
      while (flags[j]) j = (j + 1 == table_size) ? 0 : j + 1;
      flags[j] = true;
      count++;
      assign_uninitialized(table[j],
			   result_type(get_key(Heavy[0]), val));
    }

    // pack non-empty entries of table into result sequence
    // uninitialized_sequence<result_type> r(count); // breaks ??
    auto r = sequence<result_type>::uninitialized(count); 
    size_t j = 0;
    for (size_t i = 0; i < table_size; i++) {
      if (flags[i])
	move_uninitialized(r[j++], table[i]);
    }

    assert(j == count);
    return r;
  }

// this one is for more buckets than the length of A (i.e. sparse)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
template <typename Iterator, typename HashEq, typename GetK, typename GetV, typename M>
auto collect_reduce_sparse(slice<Iterator,Iterator> A,
			   HashEq hasheq, GetK get_key, GetV get_val,
			   M const &monoid) {
  using T = typename slice<Iterator, Iterator>::value_type;
  using key_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_key(A[0]))>>;
  using val_type = typename std::remove_cv_t<std::remove_reference_t<decltype(get_val(A[0]))>>;
  using result_type = std::pair<key_type,val_type>;
  
  size_t n = A.size();

  if (n < 10000) {
    auto x = seq_collect_reduce_sparse(A, A.cut(0,0), hasheq,
				       get_key, get_val, monoid);
    return map(x, [&] (auto v) {return std::move(v);});
  };
  
  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = log2_up(
      (size_t)(1 + (1.2 * 2 * sizeof(T) * n) / (float)cache_per_thread));
  bits = std::max<size_t>(bits, 4);
  size_t num_buckets = (1 << bits);

  // Returns a map (hash) from key to bucket.
  // Keys with many elements (big) have their own bucket while
  // others share a bucket.
  sequence<T> B = sequence<T>::uninitialized(n);
  uninitialized_sequence<T> Tmp(n); // = sequence<T>::uninitialized(n);

  // first buckets based on hash using a counting sort
  // Guy : could possibly just use counting sort.  Would avoid needing tmp
  get_bucket<T, key_type, HashEq, GetK> gb(A, hasheq, get_key, bits);
  sequence<size_t> bucket_offsets =
    integer_sort_<std::false_type, uninitialized_copy_tag>(
      make_slice(A), make_slice(B), make_slice(Tmp) , gb, bits, num_buckets);

  size_t num_tables = gb.heavy_hitters ? num_buckets / 2 : num_buckets;
  
  // now in parallel process each bucket sequentially, returning a packed sequence
  // of the results within that bucket
  //auto tables = sequence<uninitialized_sequence<result_type>>:: // breaks ??
  auto tables = sequence<sequence<result_type>>::
    from_function(num_tables, [&] (size_t i) {
      auto heavy = (gb.heavy_hitters ?
		    B.cut(bucket_offsets[num_tables + i], bucket_offsets[num_tables + i + 1]) :
		    B.cut(0,0));
      return seq_collect_reduce_sparse(B.cut(bucket_offsets[i],bucket_offsets[i+1]),
				       heavy, hasheq, get_key, get_val, monoid);
    }, 1);
  
  // based on sizes of each result, allocate offset in full result sequence
  auto sizes = tabulate(num_tables+1, [&] (size_t i) {
      return (i==num_tables) ? 0 : tables[i].size();});
  
  size_t total = scan_inplace(make_slice(sizes),addm<size_t>());

  // move partial result to full result based on offsets
  sequence<result_type> result = sequence<result_type>::uninitialized(total);
  auto copy_f = [&] (size_t i) {
    size_t d_offset = sizes[i];
    size_t len = sizes[i + 1] - sizes[i];
    for (size_t j = 0; j < len; j++)
      move_uninitialized(result[d_offset + j], tables[i][j]); 
  };
  parallel_for(0, num_tables, copy_f, 1);
  return result;
}

  // composes a hash function and equal function
  template <typename Hash, typename Equal>
  struct hasheq {
    Hash hash;
    Equal equal;
    hasheq(Hash hash, Equal equal) : hash(hash), equal(equal) {};
  };

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
  auto group_by_and_combine(R const &A, Monoid const &monoid,
			    Hash hash = {}, Equal equal = {}) { 
    auto get_key = [] (const auto& a) {return a.first;};
    auto get_val = [] (const auto& a) {return a.second;};
    return collect_reduce_sparse(make_slice(A), hasheq(hash,equal),
				 get_key, get_val, monoid);
  }

  // Returns a sequence of <R::value_type,size_t> pairs, each consisting of
  // a unique value from the input, and the number of times it appears.
  // Returned in an arbitrary order that depends on the hash function.
  template <PARLAY_RANGE_TYPE R,
	    typename Hash = std::hash<range_value_type_t<R>>,
	    typename Equal = std::equal_to<range_value_type_t<R>>>
  auto group_by_and_count(const R& A, Hash hash = {}, Equal equal = {}) { 
    auto get_key = [] (const auto& a) -> auto& { return a; };
    auto get_val = [] (const auto&) { return (size_t) 1; };
    
    return collect_reduce_sparse(make_slice(A), hasheq(hash, equal),
				 get_key, get_val, parlay::addm<size_t>());
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
  auto group_by_and_combine_by_bucket(R const &A, size_t num_buckets,
				      Monoid const &monoid) {
    auto get_key = [] (const auto& a) {return a.first;};
    auto get_val = [] (const auto& a) {return a.second;};
    return collect_reduce(make_slice(A), get_key, get_val, monoid, num_buckets);
  }

  // Given a sequence of integers creates a histogram with the count
  // of each interger value.   The range num_buckets must be specified
  // and it is an error if any integers is out of the range [0:num_buckets).
  template <PARLAY_RANGE_TYPE R, typename Integer_t>
  auto histogram(R const &A, Integer_t num_buckets) {
    auto get_key = [&] (const auto& a) { return a; };
    auto get_val = [&] (const auto&) { return (Integer_t) 1; };
    return collect_reduce(A, get_key, get_val, parlay::addm<Integer_t>(), num_buckets);
  }

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_COLLECT_REDUCE_H_
