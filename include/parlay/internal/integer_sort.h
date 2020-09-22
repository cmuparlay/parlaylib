
#ifndef PARLAY_INTEGER_SORT_H_
#define PARLAY_INTEGER_SORT_H_

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "counting_sort.h"
#include "sequence_ops.h"
#include "quicksort.h"

#include "../delayed_sequence.h"
#include "../slice.h"
#include "../utilities.h"

namespace parlay {
namespace internal {

constexpr size_t radix = 8;
constexpr size_t max_buckets = 1 << radix;

// Use a smaller base case threshold for debugging so that test
// cases do not need to use extremely large sequences in order
// to achieve adequate coverage.
#ifdef DEBUG
constexpr size_t PARLAY_INTEGER_SORT_BASE_CASE_SIZE = 128;
#else
constexpr size_t PARLAY_INTEGER_SORT_BASE_CASE_SIZE = 1 << 17;
#endif

// a bottom up radix sort
template <typename out_uninitialized_tag, typename InIterator, typename OutIterator, class GetKey>
void seq_radix_sort_(slice<InIterator, InIterator> In,
                     slice<OutIterator, OutIterator> Out,
                     GetKey const &g,
                     size_t bits,
                     bool inplace = true) {
  size_t n = In.size();
  if (n == 0) return;
  size_t counts[max_buckets + 1];
  bool swapped = false;
  int bit_offset = 0;
  while (bits > 0) {
    size_t round_bits = (std::min)(radix, bits);
    size_t num_buckets = (1 << round_bits);
    size_t mask = num_buckets - 1;
    
    if (swapped) {
      auto get_key = [&](size_t i) -> size_t {
        return (g(Out[i]) >> bit_offset) & mask;
      };
      seq_count_sort_<std::false_type, std::false_type>(Out, In, delayed_seq<size_t>(n, get_key), counts,
                      num_buckets);
    }
    
    else {
      auto get_key = [&](size_t i) -> size_t {
        return (g(In[i]) >> bit_offset) & mask;
      };
      if (bit_offset == 0 && out_uninitialized_tag::value == true) {
        seq_count_sort_<std::false_type, std::true_type>(In, Out, delayed_seq<size_t>(n, get_key), counts,
                      num_buckets);
      }
      else {
        seq_count_sort_<std::false_type, std::false_type>(In, Out, delayed_seq<size_t>(n, get_key), counts,
                      num_buckets);
      }
    }
                    
    bits = bits - round_bits;
    bit_offset = bit_offset + round_bits;
    swapped = !swapped;
  }
  
  if (swapped && inplace) {
    for (size_t i = 0; i < n; i++) {
      In[i] = std::move(Out[i]);
    }
  }
  else if (!swapped && !inplace) {
    for (size_t i = 0; i < n; i++) {
      Out[i] = std::move(In[i]);
    }
  }
}


// wrapper to reduce copies and avoid modifying In when not inplace
// In and Tmp can be the same, but Out must be different
template <typename inplace_tag, typename copy_tag, typename out_uninitialized_tag, typename tmp_uninitialized_tag,
          typename InIterator, typename OutIterator, typename TmpIterator, class GetKey>
void seq_radix_sort(slice<InIterator, InIterator> In,
                    slice<OutIterator, OutIterator> Out,
                    slice<TmpIterator, TmpIterator> Tmp,
                    GetKey const &g,
                    size_t key_bits) {
  if constexpr (inplace_tag::value == true) {
    static_assert(copy_tag::value == false);
    seq_radix_sort_<out_uninitialized_tag>(In, Out, g, key_bits, true);
  }
  else {
    bool odd = ((key_bits - 1) / radix) & 1;
    size_t n = In.size();
    if (odd) {
      for (size_t i = 0; i < n; i++)
        assign_dispatch(Tmp[i], In[i], copy_tag{}, tmp_uninitialized_tag());
      seq_radix_sort_<out_uninitialized_tag>(Tmp, Out, g, key_bits, false);
    } else {
      for (size_t i = 0; i < n; i++)
        assign_dispatch(Out[i], In[i], copy_tag{}, out_uninitialized_tag());
      seq_radix_sort_<tmp_uninitialized_tag>(Out, Tmp, g, key_bits, true);
    }
  }
}

// a top down recursive radix sort
// g extracts the integer keys from In
// key_bits specifies how many bits there are left
// if inplace is true, then result will be in Tmp, otherwise in Out
// In and Out cannot be the same, but In and Tmp should be same if inplace
//
// inplace_tag must be one of std::true_type or std::false_type. If inplace_tag is true_type,
// then the output of the sort will remain in In. If inplace_tag is false, then the
// output of the sort will be written into Out.
//
// copy_tag must be one of std::true_type or std::false_type. If copy_tag is true_type,
// then the contents of In are copied into the output. If copy_tag is false, then the
// contents of In are moved into the output.
//
// out_uninitialized_tag must be one of std::true_type or std::false_type. If it is
// true_type, then the memory pointed to by Out is uninitialized.
// 
// tmp_uninitialized_tag must be one of std::true_type or std::false_type. If it is
// true_type, then the memory pointed to by Tmp is uninitialized.
//
template <typename inplace_tag, typename copy_tag, typename out_uninitialized_tag, typename tmp_uninitialized_tag,
          typename InIterator, typename OutIterator, typename TmpIterator, typename Get_Key>
sequence<size_t> integer_sort_r(slice<InIterator, InIterator> In,
                                slice<OutIterator, OutIterator> Out,
                                slice<TmpIterator, TmpIterator> Tmp,
                                Get_Key const &g, size_t key_bits,
                                size_t num_buckets, bool inplace,
                                float parallelism = 1.0) {
  using T = typename slice<InIterator, InIterator>::value_type;
  
  size_t n = In.size();
  size_t cache_per_thread = 1000000;
  auto sz = 2 * (size_t)sizeof(T) * n / cache_per_thread;
  size_t base_bits = sz > 0 ? log2_up(sz) : 0;
  // keep between 8 and 13
  base_bits = std::max<size_t>(8, std::min<size_t>(13, base_bits));
  sequence<size_t> offsets;
  bool one_bucket;
  bool return_offsets = (num_buckets > 0);

  if (key_bits == 0) {
    if constexpr (inplace_tag::value == false) {
      parallel_for(0, In.size(), [&](size_t i) {
        assign_dispatch(Out[i], In[i], copy_tag(), out_uninitialized_tag());
      });
    }
    return sequence<size_t>();
  }
  // for small inputs or little parallelism use sequential radix sort
  else if ((n < PARLAY_INTEGER_SORT_BASE_CASE_SIZE || parallelism < .0001) && !return_offsets) {
    seq_radix_sort<inplace_tag, copy_tag, out_uninitialized_tag, tmp_uninitialized_tag>(
      In, Out, Tmp, g, key_bits);
    return sequence<size_t>(); 
  }
  // few bits, just do a single parallel count sort
  else if (key_bits <= base_bits) {
    size_t mask = (1 << key_bits) - 1;
    auto f = [&](size_t i) { return g(In[i]) & mask; };
    auto get_bits = delayed_seq<size_t>(n, f);
    size_t num_bkts = (num_buckets == 0) ? (1 << key_bits) : num_buckets;
    
    // only uses one bucket optimization (last argument) if inplace
    std::tie(offsets, one_bucket) =
        count_sort<copy_tag, out_uninitialized_tag>(
          In, Out, make_slice(get_bits), num_bkts, parallelism, inplace);
        
    if constexpr (inplace_tag::value == true) {
      if (!one_bucket) {
        parallel_for(0, n, [&](size_t i) {
          In[i] = std::move(Out[i]);
        });
      }
    }
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
        count_sort<copy_tag, out_uninitialized_tag>(
          In, Out, make_slice(get_bits), num_outer_buckets, parallelism, !return_offsets);

    // if all but one bucket are empty, try again on lower bits
    if (one_bucket) {
      return integer_sort_r<inplace_tag, copy_tag, out_uninitialized_tag, tmp_uninitialized_tag>(
        In, Out, Tmp, g, shift_bits, 0, inplace, parallelism);
    }
    
    // After this point, Out is guaranteed to be initialized

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
          sequence<size_t> r;

          // constexpr condition is required to avoid infinite recursion when instantiating
          // the template. Recursing on integer_sort_r<std::negation<inplace_tag>, std::false_type>
          // would be the intuitive thing to do, but it makes Clang fail.
          if constexpr (inplace_tag::value == true) {
            r = integer_sort_r<std::false_type, std::false_type, tmp_uninitialized_tag, std::false_type>(
              a, b, a, g, shift_bits, num_inner_buckets, !inplace, (parallelism * (end - start)) / (n + 1));
          }
          else {
            r = integer_sort_r<std::true_type, std::false_type, tmp_uninitialized_tag, std::false_type>(
              a, b, a, g, shift_bits, num_inner_buckets, !inplace, (parallelism * (end - start)) / (n + 1));
          }

          if (return_offsets) {
            size_t bstart = (std::min)(i * num_inner_buckets, num_buckets);
            size_t bend = (std::min)((i + 1) * num_inner_buckets, num_buckets);
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
template <typename dest_tag, typename copy_tag, typename out_uninitialized_tag, typename tmp_uninitialized_tag,
          typename InIterator, typename OutIterator, typename TmpIterator, typename Get_Key>
sequence<size_t> integer_sort_(slice<InIterator, InIterator> In,
                               slice<OutIterator, OutIterator> Out,
                               slice<TmpIterator, TmpIterator> Tmp,
                               Get_Key const &g,
                               size_t bits,
                               size_t num_buckets) {
  if (bits == 0) {
    auto get_key = [&](size_t i) { return g(In[i]); };
    auto keys = delayed_seq<size_t>(In.size(), get_key);
    bits = log2_up(internal::reduce(make_slice(keys), maxm<size_t>()) + 1);
  }
  return integer_sort_r<dest_tag, copy_tag, out_uninitialized_tag, tmp_uninitialized_tag>(
    In, Out, Tmp, g, bits, num_buckets, dest_tag::value);
}

template <typename Iterator, typename Get_Key>
void integer_sort_inplace(slice<Iterator, Iterator> In,
                          Get_Key const &g, size_t bits = 0) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto Tmp = sequence<value_type>::uninitialized(In.size());
  integer_sort_<std::true_type, std::false_type, std::true_type, std::false_type>(
    In, make_slice(Tmp), In, g, bits, 0);
}


template <typename Iterator, typename Get_Key>
auto integer_sort(slice<Iterator, Iterator> In,
                  Get_Key const &g,
                  size_t bits = 0) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto Out = sequence<value_type>::uninitialized(In.size());
  auto Tmp = sequence<value_type>::uninitialized(In.size());
  integer_sort_<std::false_type, std::true_type, std::true_type, std::true_type>(
    In, make_slice(Out), make_slice(Tmp), g, bits, 0);
  return Out;
}

// Given a sorted sequence of integers in the range [0,..,num_buckets)
// returns a sequence of length num_buckets+1 with the offset for the
// start of each integer.   If an integer does not appear, its offset
// will be the same as the next (i.e. offset[i+1]-offset[i] specifies
// how many i there are.
// The last element contains the size of the input.
template <typename Tint = size_t, typename Iterator, typename Get_Key>
sequence<Tint> get_counts(slice<Iterator, Iterator> In, Get_Key const &g, size_t num_buckets) {
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
  return sequence<Tint>::from_function(num_buckets,
                        [&](size_t i) { return ends[i] - starts[i]; });
}

template <typename Tint = size_t, typename Iterator, typename Get_Key>
auto integer_sort_with_counts(slice<Iterator, Iterator> In, Get_Key const &g, size_t num_buckets) {
  size_t bits = log2_up(num_buckets);
  auto R = integer_sort(In, g, bits);
  return std::make_pair(std::move(R), get_counts<Tint>(make_slice(R), g, num_buckets));
}

}
}  // namespace parlay

#endif  // PARLAY_INTEGER_SORT_H_
