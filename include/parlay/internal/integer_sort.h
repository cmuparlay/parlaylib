
#ifndef PARLAY_INTEGER_SORT_H_
#define PARLAY_INTEGER_SORT_H_

#include <cassert>
#include <cstdio>

#include <algorithm>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "counting_sort.h"
#include "sequence_ops.h"
#include "uninitialized_sequence.h"
#include "get_time.h"

#include "../delayed_sequence.h"
#include "../monoid.h"
#include "../parallel.h"
#include "../range.h"
#include "../relocation.h"
#include "../sequence.h"
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
template <typename InIterator, typename OutIterator, class GetKey>
void seq_radix_sort_(slice<InIterator, InIterator> In,
                     slice<OutIterator, OutIterator> Out,
                     GetKey const &g,
                     size_t bits,
                     bool inplace = true) {
  size_t n = In.size();
  if (n == 0) return;
  size_t counts[max_buckets + 1];
  bool swapped = false;
  size_t bit_offset = 0;
  while (bits > 0) {
    size_t round_bits = (std::min)(radix, bits);
    size_t num_buckets = (size_t{1} << round_bits);
    size_t mask = num_buckets - 1;
    
    if (swapped) {
      auto get_key = [&](size_t i) -> size_t {
        return (g(Out[i]) >> bit_offset) & mask;
      };
      seq_count_sort_<uninitialized_relocate_tag>(Out, In, delayed_seq<size_t>(n, get_key), counts, num_buckets);
    }
    
    else {
      auto get_key = [&](size_t i) -> size_t {
        return (g(In[i]) >> bit_offset) & mask;
      };

      seq_count_sort_<uninitialized_relocate_tag>(In, Out, delayed_seq<size_t>(n, get_key), counts, num_buckets);
      
    }
                    
    bits = bits - round_bits;
    bit_offset = bit_offset + round_bits;
    swapped = !swapped;
  }
  
  if (swapped && inplace) {
    uninitialized_relocate_n(In.begin(), Out.begin(), In.size());
  }
  else if (!swapped && !inplace) {
    uninitialized_relocate_n(Out.begin(), In.begin(), Out.size());
  }
}


// wrapper to reduce copies and avoid modifying In when not inplace
// In and Tmp can be the same, but Out must be different
template <typename inplace_tag, typename assignment_tag, typename InIterator, typename OutIterator, typename TmpIterator, class GetKey>
void seq_radix_sort(slice<InIterator, InIterator> In,
                    slice<OutIterator, OutIterator> Out,
   [[maybe_unused]] slice<TmpIterator, TmpIterator> Tmp,
                    GetKey const &g,
                    size_t key_bits) {
  if constexpr (inplace_tag::value == true) {
    static_assert(std::is_same_v<assignment_tag, uninitialized_relocate_tag>);
    seq_radix_sort_(In, Out, g, key_bits, true);
  }
  else {
    bool odd = ((key_bits - 1) / radix) & 1;
    size_t n = In.size();
    if (odd) {
      // We could just use assign_dispatch(Tmp[i], In[i]) for each i, but we
      // can optimize better by calling destructive_move_slice, since this
      // has the ability to memcpy multiple elements at once
      if constexpr (std::is_same_v<assignment_tag, uninitialized_relocate_tag>) {
        uninitialized_relocate_n(Tmp.begin(), In.begin(), Tmp.size());
      }
      else {
        for (size_t i = 0; i < n; i++)
          assign_uninitialized(Tmp[i], In[i]);
      }
      seq_radix_sort_(Tmp, Out, g, key_bits, false);
    } else {
      if constexpr (std::is_same_v<assignment_tag, uninitialized_relocate_tag>) {
        uninitialized_relocate_n(Out.begin(), In.begin(), Out.size());
      }
      else {
        for (size_t i = 0; i < n; i++)
          assign_uninitialized(Out[i], In[i]);
      }
      seq_radix_sort_(Out, Tmp, g, key_bits, true);
    }
  }
}

// a top down recursive radix sort
// g extracts the integer keys from In
// key_bits specifies how many bits there are left
//
// inplace_tag must be one of std::true_type or std::false_type. If inplace_tag is true_type,
// then the output of the sort will remain in In. If inplace_tag is false, then the
// output of the sort will be written into Out. Furthermore, if inplace_tag is std::true_type,
// then Tmp must point to the same range as In
//
// assignment_tag must be one of uninitialized_copy_tag or uninitialized_relocate_tag. This indicates
// the manner in which the input is moved from In to Out. If inplace_tag is std::true_type,
// then this must always be uninitialized_relocate_tag. If inplace_tag is std::false_type, then
// assignment_tag can be either uninitialized_copy_tag, if the input is to be copied from
// In to Out, or uninitialized_relocate_tag if the input is to be (destructively) moved from In to
// Out.
//
template <typename inplace_tag, typename assignment_tag,
          typename InIterator, typename OutIterator, typename TmpIterator, typename Get_Key>
sequence<size_t> integer_sort_r(slice<InIterator, InIterator> In,
                                slice<OutIterator, OutIterator> Out,
                                slice<TmpIterator, TmpIterator> Tmp,
                                Get_Key const &g, size_t key_bits,
                                size_t num_buckets,
                                float parallelism = 1.0) {
  // Parameter In
  //  [Preconditions]
  //  - In owns valid initialized memory
  //  - If inplace_tag is std::true_type, then In points to mutable (non-const) memory
  //  - If assignment_tag is uninitialized_copy_tag, then In points to objects of a type
  //    that is copy constructible
  //  [Postconditions]
  //  - If inplace_tag is std::true_type, then In contains the sorted results
  //  - If inplace_tag is std::false_type and assignment_tag is uninitialized_copy_tag,
  //    then In contains the original unmodified input
  //  - If inplace_tag is std::false_type and assignment_tag is uninitialized_relocate_tag, then
  //    In points to uninitialized memory
  //
  // Parameter Out
  //  [Preconditions]
  //  - Out points to uninitialized memory
  //  [Postconditions]
  //  - If inplace_tag is std::true_type, then Out points to uninitialized memory
  //  - If inplace_tag is std::false_type, then Out points to the resulting sorted objects  
  //
  // Parameter Tmp
  //  [Preconditions]
  //  - If inplace_tag is std::true_type, then Tmp points to the same range as In
  //  - If inplace_tag is std::false_type and assignment_tag is uninitialized_copy_tag,
  //    then Tmp points to uninitialized memory
  //  - If inplace_tag is std::false_type and assignment_tag is uninitialized_relocate_tag,
  //    then Tmp points to the same range as In
  //  [Postconditions]
  //  - If Tmp does not point to the same range as In, then it points to uninitialized memory

  // Ranges should all have the same underlying type
  static_assert(std::is_same_v<range_value_type_t<slice<InIterator, InIterator>>,
                               range_value_type_t<slice<OutIterator, OutIterator>>>);
  static_assert(std::is_same_v<range_value_type_t<slice<InIterator, InIterator>>,
                               range_value_type_t<slice<TmpIterator, TmpIterator>>>);
                               
  // assignment_type can only be one of uninitialized_copy_tag or uninitialized_relocate_tag
  static_assert(std::is_same_v<assignment_tag, uninitialized_copy_tag> || std::is_same_v<assignment_tag, uninitialized_relocate_tag>);
  
  // assignment_tag is only allowed to be uninitialized_copy_tag if inplace_tag is std::false type
  static_assert(inplace_tag::value == false || std::is_same_v<assignment_tag, uninitialized_relocate_tag>);
  

  using T = typename slice<InIterator, InIterator>::value_type;
  timer t("integer sort", false);
  
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
    // If the sort is not inplace, the final result needs to
    // be moved into Out since it is currently in In.
    if constexpr (inplace_tag::value == false) {

      // We could just do a parallel for loop and copy/move the elements from
      // In to Out using assign_dispatch(Out[i], In[i], assignment_tag()), but
      // this would not allow us to take advantage of the possibly more efficient
      // uninitialized_relocate_n, which can memcpy multiple elements at a time
      // to save on performing every copy individually.
      if constexpr (std::is_same_v<assignment_tag, uninitialized_relocate_tag>) {
        uninitialized_relocate_n(Out.begin(), In.begin(), Out.size());
      }
      else {
        parallel_for(0, In.size(), [&](size_t i) {
          assign_uninitialized(Out[i], In[i]);     // Copy from In[i] to Out[i]
        });
      }  
    }
    return sequence<size_t>();
  }
  // for small inputs or little parallelism use sequential radix sort
  else if ((n < PARLAY_INTEGER_SORT_BASE_CASE_SIZE || parallelism < .0001) && !return_offsets) {
    seq_radix_sort<inplace_tag, assignment_tag>(In, Out, Tmp, g, key_bits);
    return sequence<size_t>(); 
  }
  // few bits, just do a single parallel count sort
  else if (key_bits <= base_bits) {
    size_t mask = (1 << key_bits) - 1;
    auto f = [&](size_t i) { return static_cast<size_t>(g(In[i]) & mask); };
    auto get_bits = delayed_seq<size_t>(n, f);
    size_t num_bkts = (num_buckets == 0) ? (size_t{1} << key_bits) : num_buckets;
    
    // only uses one bucket optimization (last argument) if inplace
    std::tie(offsets, one_bucket) = count_sort<assignment_tag>(In, Out,
      make_slice(get_bits), num_bkts, parallelism, inplace_tag::value);
    t.next("count sort");
  
    if constexpr (inplace_tag::value == true) {
      if (!one_bucket) {
        uninitialized_relocate_n(In.begin(), Out.begin(), In.size());
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
    size_t num_outer_buckets = (size_t{1} << bits);
    size_t num_inner_buckets = return_offsets ? ((size_t)1 << shift_bits) : 0;
    size_t mask = num_outer_buckets - 1;
    auto f = [&](size_t i) { return static_cast<size_t>((g(In[i]) >> shift_bits) & mask); };
    auto get_bits = delayed_seq<size_t>(n, f);

    // divide into buckets
    std::tie(offsets, one_bucket) = count_sort<assignment_tag>(In,
        Out, make_slice(get_bits), num_outer_buckets, parallelism, !return_offsets);
    if (parallelism == 1.0) t.next("recursive count sort");

    // if all but one bucket are empty, try again on lower bits
    if (one_bucket) {
      return integer_sort_r<inplace_tag, assignment_tag>(In, Out, Tmp, g, shift_bits, 0, parallelism);
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

          auto new_parallelism = (parallelism * static_cast<float>(end - start)) / static_cast<float>(n + 1);
          r = integer_sort_r<typename std::negation<inplace_tag>::type, uninitialized_relocate_tag>(
            a, b, a, g, shift_bits, num_inner_buckets, new_parallelism);

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
// bits specifies how many bits there are in the key
//    if set to 0, then a max is taken over the keys to determine
// If num_buckets is non-zero then the output sequence will contain
// the offsets of each bucket (num_bucket of them)
// num_bucket must be less than or equal to 2^bits
template <typename inplace_tag, typename assignment_tag, typename InIterator, typename OutIterator, typename TmpIterator, typename Get_Key>
sequence<size_t> integer_sort_(slice<InIterator, InIterator> In,
                               slice<OutIterator, OutIterator> Out,
                               slice<TmpIterator, TmpIterator> Tmp,
                               Get_Key const &g,
                               size_t bits,
                               size_t num_buckets) {
  if (bits == 0) {
    auto get_key = [&](size_t i) { return static_cast<size_t>(g(In[i])); };
    auto keys = delayed_seq<size_t>(In.size(), get_key);
    bits = log2_up(internal::reduce(make_slice(keys), maximum<size_t>()) + 1);
  }
  return integer_sort_r<inplace_tag, assignment_tag>(
    In, Out, Tmp, g, bits, num_buckets);
}

template <typename Iterator, typename Get_Key>
void integer_sort_inplace(slice<Iterator, Iterator> In,
                          Get_Key const &g, size_t bits = 0) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto Tmp = internal::uninitialized_sequence<value_type>(In.size());
  integer_sort_<std::true_type, uninitialized_relocate_tag>(In, make_slice(Tmp), In, g, bits, 0);
}


template <typename Iterator, typename Get_Key>
auto integer_sort(slice<Iterator, Iterator> In,
                  Get_Key const &g,
                  size_t bits = 0) {
  using value_type = typename slice<Iterator, Iterator>::value_type;
  auto Out = sequence<value_type>::uninitialized(In.size());
  auto Tmp = uninitialized_sequence<value_type>(In.size());
  integer_sort_<std::false_type, uninitialized_copy_tag>(
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
  if (n == 0) {
    return {};
  }
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
  using T = typename slice<Iterator, Iterator>::value_type;
  if (In.size() == 0) {
    return std::make_pair(parlay::sequence<T>{}, parlay::sequence<Tint>(num_buckets));
  }
  assert(num_buckets > 0);
  size_t bits = log2_up(num_buckets);
  auto R = integer_sort(In, g, bits);
  return std::make_pair(std::move(R), get_counts<Tint>(make_slice(R), g, num_buckets));
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTEGER_SORT_H_
