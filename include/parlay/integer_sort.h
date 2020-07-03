
#ifndef PARLAY_INTEGER_SORT_H_
#define PARLAY_INTEGER_SORT_H_

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "counting_sort.h"
#include "delayed_sequence.h"
#include "range.h"
#include "quicksort.h"
#include "utilities.h"

namespace parlay {

constexpr size_t radix = 8;
constexpr size_t max_buckets = 1 << radix;

// a bottom up radix sort
template <class Slice, class GetKey>
void seq_radix_sort_(Slice In, Slice Out, GetKey const &g, size_t bits,
                     bool inplace = true) {
  size_t n = In.size();
  if (n == 0) return;
  size_t counts[max_buckets + 1];
  bool swapped = false;
  int bit_offset = 0;
  while (bits > 0) {
    size_t round_bits = std::min(radix, bits);
    size_t num_buckets = (1 << round_bits);
    size_t mask = num_buckets - 1;
    auto get_key = [&](size_t i) -> size_t {
      return (g(In[i]) >> bit_offset) & mask;
    };
    seq_count_sort_(In, Out, delayed_seq<size_t>(n, get_key), counts,
                    num_buckets);
    std::swap(In, Out);
    bits = bits - round_bits;
    bit_offset = bit_offset + round_bits;
    swapped = !swapped;
  }
  if ((inplace && swapped) || (!inplace && !swapped)) {
    for (size_t i = 0; i < n; i++) move_uninitialized(Out[i], In[i]);
  }
}

// wrapper to reduce copies and avoid modifying In when not inplace
// In and Tmp can be the same, but Out must be different
template <typename InIterator, typename OutIterator, typename TmpIterator, class GetKey>
void seq_radix_sort(slice<InIterator, InIterator> In,
                    slice<OutIterator, OutIterator> Out,
                    slice<TmpIterator, TmpIterator> Tmp,
                    GetKey const &g,
                    size_t key_bits, bool inplace = true) {
  bool odd = ((key_bits - 1) / radix) & 1;
  size_t n = In.size();
  if (In == Tmp) {  // inplace
    seq_radix_sort_(Tmp, Out, g, key_bits, inplace);
  } else {
    if (odd) {
      for (size_t i = 0; i < n; i++) move_uninitialized(Tmp[i], In[i]);
      seq_radix_sort_(Tmp, Out, g, key_bits, false);
    } else {
      for (size_t i = 0; i < n; i++) move_uninitialized(Out[i], In[i]);
      seq_radix_sort_(Out, Tmp, g, key_bits, true);
    }
  }
}

// a top down recursive radix sort
// g extracts the integer keys from In
// key_bits specifies how many bits there are left
// if inplace is true, then result will be in Tmp, otherwise in Out
// In and Out cannot be the same, but In and Tmp should be same if inplace
template <typename InIterator, typename OutIterator, typename TmpIterator, typename Get_Key>
sequence<size_t> integer_sort_r(slice<InIterator, InIterator> In,
                                slice<OutIterator, OutIterator> Out,
                                slice<TmpIterator, TmpIterator> Tmp,
                                Get_Key const &g, size_t key_bits,
                                size_t num_buckets, bool inplace,
                                float parallelism = 1.0) {
  using T = typename slice<InIterator, InIterator>::value_type;
  
  size_t n = In.size();
  size_t cache_per_thread = 1000000;
  size_t base_bits = log2_up(2 * (size_t)sizeof(T) * n / cache_per_thread);
  // keep between 8 and 13
  base_bits = std::max<size_t>(8, std::min<size_t>(13, base_bits));
  sequence<size_t> offsets;
  bool one_bucket;
  bool return_offsets = (num_buckets > 0);

  if (key_bits == 0) {
    if (!inplace) parallel_for(0, In.size(), [&](size_t i) { Out[i] = In[i]; });
    return sequence<size_t>();

    // for small inputs or little parallelism use sequential radix sort
  } else if ((n < (1 << 17) || parallelism < .0001) && !return_offsets) {
    seq_radix_sort(In, Out, Tmp, g, key_bits, inplace);
    return sequence<size_t>();

    // few bits, just do a single parallel count sort
  } else if (key_bits <= base_bits) {
    size_t mask = (1 << key_bits) - 1;
    auto f = [&](size_t i) { return g(In[i]) & mask; };
    auto get_bits = delayed_seq<size_t>(n, f);
    size_t num_bkts = (num_buckets == 0) ? (1 << key_bits) : num_buckets;
    // only uses one bucket optimization (last argument) if inplace
    std::tie(offsets, one_bucket) =
        count_sort(In, Out, make_slice(get_bits), num_bkts, parallelism, inplace);
    if (inplace && !one_bucket)
      parallel_for(0, n, [&](size_t i) { move_uninitialized(Tmp[i], Out[i]); });
    if (return_offsets)
      return offsets;
    else
      return sequence<size_t>();

    // recursive case
  } else {
    size_t bits = 8;
    size_t shift_bits = key_bits - bits;
    size_t num_outer_buckets = (1 << bits);
    size_t num_inner_buckets = return_offsets ? ((size_t)1 << shift_bits) : 0;
    size_t mask = num_outer_buckets - 1;
    auto f = [&](size_t i) { return (g(In[i]) >> shift_bits) & mask; };
    auto get_bits = delayed_seq<size_t>(n, f);

    // divide into buckets
    std::tie(offsets, one_bucket) =
        count_sort(In, Out, make_slice(get_bits), num_outer_buckets, parallelism,
                   !return_offsets);

    // if all but one bucket are empty, try again on lower bits
    if (one_bucket) {
      return integer_sort_r(In, Out, Tmp, g, shift_bits, 0, inplace,
                            parallelism);
    }

    sequence<size_t> inner_offsets(return_offsets ? num_buckets + 1 : 0);
    if (return_offsets) inner_offsets[num_buckets] = n;

    // recursively sort each bucket
    parallel_for(
        0, num_outer_buckets,
        [&](size_t i) {
          size_t start = offsets[i];
          size_t end = offsets[i + 1];
          auto a = Out.cut(start, end);
          auto b = Tmp.cut(start, end);
          sequence<size_t> r =
              integer_sort_r(a, b, a, g, shift_bits, num_inner_buckets,
                             !inplace, (parallelism * (end - start)) / (n + 1));
          if (return_offsets) {
            size_t bstart = std::min(i * num_inner_buckets, num_buckets);
            size_t bend = std::min((i + 1) * num_inner_buckets, num_buckets);
            size_t m = (bend - bstart);
            for (size_t j = 0; j < m; j++)
              inner_offsets[bstart + j] = offsets[i] + r[j];
          }
        },
        1);
    return inner_offsets;
  }
}

// a top down recursive radix sort
// g extracts the integer keys from In
// if inplace is false then result will be placed in Out,
//    otherwise they are placed in Tmp
//    Tmp and In can be the same (i.e. to do inplace set them equal)
// In is not directly modified, but can be indirectly if equal to Tmp
// val_bits specifies how many bits there are in the key
//    if set to 0, then a max is taken over the keys to determine
// If num_buckets is non-zero then the output sequence will contain
// the offsets of each bucket (num_bucket of them)
// num_bucket must be less than or equal to 2^bits
template <typename SeqIn, typename OutIterator, typename Get_Key>
sequence<size_t> integer_sort_(SeqIn const &In,
                               slice<OutIterator, OutIterator> Out,
                               slice<OutIterator, OutIterator> Tmp,
                               Get_Key const &g,
                               size_t bits,
                               size_t num_buckets,
                               bool inplace) {
  if (In == Out)
    throw std::invalid_argument(
        "in integer_sort : input and output must be different locations");
  if (bits == 0) {
    auto get_key = [&](size_t i) { return g(In[i]); };
    auto keys = delayed_seq<size_t>(In.size(), get_key);
    // num_buckets = reduce(keys, maxm<size_t>()) + 1;
    // bits = log2_up(num_buckets);
    bits = log2_up(reduce(keys, maxm<size_t>()) + 1);
  }
  return integer_sort_r(In, Out, Tmp, g, bits, num_buckets, inplace);
}

template <typename Iterator, typename Get_Key>
void integer_sort_inplace(slice<Iterator, Iterator> In,
                          Get_Key const &g, size_t bits = 0) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto Tmp = sequence<value_type>::uninitialized(In.size());
  integer_sort_(In, Tmp.slice(), In, g, bits, 0, true);
}

template <typename Seq, typename Get_Key>
auto integer_sort(Seq const &In,
                  Get_Key const &g,
                  size_t bits = 0) {
  using value_type = range_value_type_t<Seq>;
  auto Out = sequence<value_type>::uninitialized(In.size());
  auto Tmp = sequence<value_type>::uninitialized(In.size());
  integer_sort_(make_slice(In), make_slice(Out), make_slice(Tmp), g, bits, 0, false);
  return Out;
}

// Given a sorted sequence of integers in the range [0,..,num_buckets)
// returns a sequence of length num_buckets+1 with the offset for the
// start of each integer.   If an integer does not appear, its offset
// will be the same as the next (i.e. offset[i+1]-offset[i] specifies
// how many i there are.
// The last element contains the size of the input.
template <typename Tint = size_t, typename Seq, typename Get_Key>
sequence<Tint> get_counts(Seq const &In, Get_Key const &g, size_t num_buckets) {
  size_t n = In.size();
  sequence<Tint> starts(num_buckets, (Tint)0);
  sequence<Tint> ends(num_buckets, (Tint)0);
  parallel_for(0, n - 1, [&](size_t i) {
    if (g(In[i]) != g(In[i + 1])) {
      starts[g(In[i + 1])] = i + 1;
      ends[g(In[i])] = i + 1;
    };
  });
  ends[g(In[n - 1])] = n;
  return sequence<Tint>(num_buckets,
                        [&](size_t i) { return ends[i] - starts[i]; });
}

template <typename Tint = size_t, typename Seq, typename Get_Key>
std::pair<sequence<typename Seq::value_type>, sequence<Tint>>
integer_sort_with_counts(Seq const &In, Get_Key const &g, size_t num_buckets) {
  size_t bits = log2_up(num_buckets);
  auto R = integer_sort(In, g, bits);
  return std::make_pair(std::move(R), get_counts<Tint>(R, g, num_buckets));
}

}  // namespace parlay

#endif  // PARLAY_INTEGER_SORT_H_
