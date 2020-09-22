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
#include "sequence_ops.h"
#include "transpose.h"

#include "../utilities.h"

namespace parlay {
namespace internal {

// the following parameters can be tuned
constexpr const size_t CR_SEQ_THRESHOLD = 8192;

template <typename Seq, typename OutSeq, class Key, class Value, typename M>
void seq_collect_reduce_few(Seq const &A, OutSeq &&Out, Key const &get_key,
                            Value const &get_value, M const &monoid,
                            size_t num_buckets) {
  size_t n = A.size();
  for (size_t i = 0; i < num_buckets; i++) Out[i] = monoid.identity;
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
  sequence<val_type> Out(num_buckets);
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

  num_blocks = 1 << log2_up(num_blocks);

  sequence<val_type> Out(num_buckets);

  // if insufficient parallelism, do sequentially
  if (n < CR_SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1)
    return seq_collect_reduce_few(A, get_key, get_value, monoid, num_buckets);

  size_t block_size = ((n - 1) / num_blocks) + 1;
  size_t m = num_blocks * num_buckets;

  sequence<val_type> OutM(m);

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
template <typename E, typename HashEq>
struct get_bucket {
  using HE = std::pair<E, int>;
  sequence<HE> hash_table;
  size_t table_mask;
  size_t bucket_mask;
  size_t num_buckets;
  bool heavy_hitters;
  const HashEq heq;

  // creates a structure from a sequence of elements
  // bits is the number of bits that will be returned by the hash function
  // items that appear many times will be mapped individually into
  //    the top half [2^{bits-1},2^{bits})
  //    and light items shared into the bottom half [0,2^{bits-1})
  template <typename Seq>
  get_bucket(Seq const &A, HashEq const &heq, size_t bits) : heq(heq) {
    size_t n = A.size();
    size_t low_bits = bits - 1;   // for the bottom half
    num_buckets = 1 << low_bits;  // in bottom half
    size_t count = 2 * num_buckets;
    size_t table_size = 4 * count;
    table_mask = table_size - 1;

    hash_table = sequence<HE>(table_size, std::make_pair(E(), -1));

    // insert sample into hash table with one less than the
    // count of how many times appears (since it starts with -1)
    for (size_t i = 0; i < count; i++) {
      E s = A[hash64(i) % n];
      size_t idx = heq.hash(s) & table_mask;
      while (1) {
        if (hash_table[idx].second == -1) {
          hash_table[idx] = std::make_pair(s, 0);
          break;
        } else if (heq.eql(hash_table[idx].first, s)) {
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
        E key = hash_table[i].first;
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
  size_t operator()(E v) const {
    if (heavy_hitters) {
      auto h = hash_table[heq.hash(v) & table_mask];
      if (h.second != -1 && heq.eql(h.first, v))
        return h.second + num_buckets;  // top half
    }
    return heq.hash(v) & bucket_mask;  // bottom half
  }
};

template <typename E, typename Key>
struct hasheq_mask_low {
  Key get_key;
  hasheq_mask_low(Key get_key) : get_key(get_key) {}
  inline size_t hash(E a) const { return hash64_2(get_key(a) & ~((size_t)15)); }
  inline bool eql(E a, E b) const { return get_key(a) == get_key(b); }
};

template <typename Seq, class Key, class Value, typename M>
auto collect_reduce(Seq const &A, Key const &get_key, Value const &get_value,
                    M const &monoid, size_t num_buckets)
    -> sequence<decltype(get_value(A[0]))> {
  using T = typename Seq::value_type;
  using val_type = decltype(get_value(A[0]));
  size_t n = A.size();

  // #bits is selected so each block fits into L3 cache
  //   assuming an L3 cache of size 1M per thread
  // the counting sort uses 2 x input size due to copy
  size_t cache_per_thread = 1000000;
  size_t bits = std::max<size_t>(
      log2_up(1 + 2 * (size_t)sizeof(val_type) * n / cache_per_thread), 4);

  size_t num_blocks = (1 << bits);

  if (num_buckets <= 4 * num_blocks)
    return collect_reduce_few(A, get_key, get_value, monoid, num_buckets);

  // Returns a map (hash) from key to block.
  // Keys with many elements (big) have their own block while
  // others share a block.
  // Keys that share low 4 bits get same block unless big.
  // This is to avoid false sharing.
  // auto get_i = [&] (size_t i) -> size_t {return A[i].first;};
  // auto s = delayed_seq<size_t>(n,get_i);

  using hasheq = hasheq_mask_low<T, Key>;
  get_bucket<T, hasheq> gb(A, hasheq(get_key), bits);
  sequence<T> B = sequence<T>::uninitialized(n);
  sequence<T> Tmp = sequence<T>::uninitialized(n);

  // first partition into blocks based on hash using a counting sort
  sequence<size_t> block_offsets;
  block_offsets = integer_sort_<std::false_type, std::true_type, std::true_type, std::true_type>(
    make_slice(A), make_slice(B), make_slice(Tmp) , gb, bits, num_blocks);

  // note that this is cache line alligned
  sequence<val_type> sums(num_buckets, monoid.identity);

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

// histogram based on collect_reduce.
// m is the number of buckets
// the output type of each bucket will have the same integer type as m
template <typename Iterator, typename Integer_>
auto histogram(slice<Iterator, Iterator> A, Integer_ m) {
  auto get_key = [&](const auto& a) { return a; };
  auto get_val = [&](const auto&) { return (Integer_)1; };
  return collect_reduce(A, get_key, get_val, parlay::addm<Integer_>(), m);
}

// this one is for more buckets than the length of A (i.e. sparse)
//  A is a sequence of key-value pairs
//  monoid has fields m.identity and m.f (a binary associative function)
template <typename Seq, typename HashEq, typename M>
sequence<typename Seq::value_type> collect_reduce_sparse(Seq const &A,
                                                         HashEq hasheq,
                                                         M const &monoid) {
  using T = typename Seq::value_type;
  using val_type = typename T::second_type;

  size_t n = A.size();

  if (n < 1000) {
    auto cmp = [](T a, T b) { return a.first < b.first; };
    sequence<T> B = sample_sort(A, cmp);
    size_t j = 0;
    for (size_t i = 1; i < n; i++) {
      if (B[i].first == B[j].first)
        B[j].second = monoid.f(B[j].second, B[i].second);
      else
        B[j++] = B[i];
    };
    return sequence<T>(j, [&](size_t i) { return B[i]; });
  }

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
  // Keys that share low 4 bits get same bucket unless big.
  // This is to avoid false sharing.
  sequence<T> B = sequence<T>::uninitialized(n);
  sequence<T> Tmp = sequence<T>::uninitialized(n);

  // first buckets based on hash using a counting sort
  get_bucket<T, HashEq> gb(A, hasheq, bits);
  sequence<size_t> bucket_offsets = integer_sort_(
      make_slice(A), make_slice(B), make_slice(Tmp), gb, bits, num_buckets, false);

  // note that this is cache line alligned
  size_t num_tables = gb.heavy_hitters ? num_buckets / 2 : num_buckets;
  size_t bucket_size = (n - 1) / num_tables + 1;
  float factor = 1.2;
  if (bucket_size < 128000) factor += (17 - log2_up(bucket_size)) * .15;
  size_t table_size = (factor * bucket_size);
  size_t total_table_size = table_size * num_tables;
  sequence<T> table = sequence<T>::uninitialized(total_table_size);
  sequence<size_t> sizes(num_tables + 1);

  // now in parallel process each bucket sequentially
  parallel_for(
      0, num_tables,
      [&](size_t i) {
        T *my_table = table.begin() + i * table_size;

        sequence<bool> flags(table_size, false);
        // clear tables
        // for (size_t i = 0; i < table_size; i++)
        // assign_uninitialized(my_table[i], T(empty, identity));

        // insert small bucket (ones with multiple different items)
        size_t start = bucket_offsets[i];
        size_t end = bucket_offsets[i + 1];
        if ((end - start) > table_size)
          throw std::runtime_error("hash table overflow in collect_reduce");
        for (size_t j = start; j < end; j++) {
          size_t idx = B[j].first;
          size_t k = ((size_t)hasheq.hash(B[j])) % table_size;
          while (flags[k] && my_table[k].first != idx)
            k = (k + 1 == table_size) ? 0 : k + 1;
          if (flags[k])
            my_table[k] = T(idx, monoid.f(my_table[k].second, B[j].second));
          else {
            flags[k] = true;
            assign_uninitialized(my_table[k], T(idx, B[j].second));
          }
        }

        // now if there are any "heavy hitters" (buckets with a single item)
        // insert them
        if (gb.heavy_hitters) {
          size_t start_l = bucket_offsets[num_tables + i];
          size_t len = bucket_offsets[num_tables + i + 1] - start_l;
          if (len > 0) {
            auto f = [&](size_t i) -> val_type {
              return B[i + start_l].second;
            };
            auto s = delayed_seq<val_type>(len, f);
            val_type x = reduce(s, monoid);
            size_t j = 0;
            while (flags[j]) j = (j + 1 == table_size) ? 0 : j + 1;
            assign_uninitialized(my_table[j], T(B[start_l].first, x));
          }
        }

        // pack tables down to bottom
        size_t j = 0;
        for (size_t i = 0; i < table_size; i++)
          if (flags[i]) move_uninitialized(my_table[j++], my_table[i]);
        sizes[i] = j;

      },
      0);

  sizes[num_tables] = 0;
  size_t total = scan_inplace(make_slice(sizes), addm<size_t>());

  // copy packed tables into contiguous result
  sequence<T> result = sequence<T>::uninitialized(total);
  auto copy_f = [&](size_t i) {
    size_t d_offset = sizes[i];
    size_t s_offset = i * table_size;
    size_t len = sizes[i + 1] - sizes[i];
    for (size_t j = 0; j < len; j++)
      move_uninitialized(result[d_offset + j], table[s_offset + j]);
  };
  parallel_for(0, num_tables, copy_f, 1);
  return result;
}

// default hash and equality
template <typename Seq, typename M>
sequence<typename Seq::value_type> collect_reduce_sparse(Seq const &A,
                                                         M const &monoid) {
  using P = typename Seq::value_type;
  struct hasheq {
    static inline size_t hash(P a) { return parlay::hash64_2(a.first); }
    static inline bool eql(P a, P b) { return a.first == b.first; }
  };
  return collect_reduce_sparse(A, hasheq(), monoid);
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_COLLECT_REDUCE_H_


