#ifndef PARLAY_COLLECT_REDUCE_H_
#define PARLAY_COLLECT_REDUCE_H_

#include <cassert>
#include <cstdio>

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "../parallel.h"
#include "../sequence.h"
#include "../slice.h"
#include "../utilities.h"

#include "counting_sort.h"
#include "sequence_ops.h"
#include "integer_sort.h"
#include "uninitialized_sequence.h"
#include "get_time.h"

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
  size_t num_blocks_ = (std::min)(4 * num_threads, n / num_buckets / 64) + 1;
  size_t block_size = ((n - 1) / num_blocks_) + 1;
  size_t num_blocks = (1 + ((n)-1) / (block_size));

  // if insufficient parallelism, do sequentially
  if (n < CR_SEQ_THRESHOLD || num_blocks == 1 || num_threads == 1)
    return seq_collect_reduce(A, helper, num_buckets);

  // partial results for each block
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
      const auto& a = A[hash64(i) % n];
      const auto& s = hasheq.get_key(a);
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
  auto Tmp = uninitialized_sequence<T>(n);

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
  size_t table_size = 3 * A.size() / 2;
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
    const auto& aj = A[j];
    const auto& key = helper.get_key(aj);
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
  size_t bits = log2_up(static_cast<size_t>(
      1 + (1.2 * 2 * sizeof(in_type) * static_cast<double>(n)) / static_cast<double>(cache_per_thread)));
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

}  // namespace parlay

#endif  // PARLAY_COLLECT_REDUCE_H_
