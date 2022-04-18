
#ifndef PARLAY_RANDOM_H_
#define PARLAY_RANDOM_H_

#include <cstddef>
#include <cstdint>

#include <tuple>
#include <limits>

#include "delayed_sequence.h"
#include "parallel.h"
#include "range.h"
#include "sequence.h"
#include "slice.h"
#include "utilities.h"

#include "internal/counting_sort.h"

// IWYU pragma: no_include <sstream>

namespace parlay {

// A deterministic random bit generator satisfying uniform_random_bit_generator
// Can therefore be used as the engine for C++'s <random> number generators.
// For example, to generate uniform random integers, write
//
//    parlay::random_generator gen;
//    std::uniform_int_distribution<vertex> dis(0, n-1);
//
// then generate random numbers by invoking dis(gen). Note that each generator
// should only be used by one thread at a time (they are not thread safe).
//
// To generate random numbers in parallel, the generator supports an operator[],
// which creates another generator seeded from the current state and a given seed.
// For example, to generate random numbers in parallel, you could write
//
//    parlay::random_generator gen;
//    std::uniform_int_distribution<vertex> dis(0, n-1);
//    auto result = parlay::tabulate(n, [&](size_t i) {
//      auto r = gen[i];
//      return dis(r);
//    });
//
// The quality of randomness should be good enough for simple randomized algorithms,
// but should not be relied upon for anything that requires high-quality randomness.
//
struct random_generator {
 public:
  using result_type = size_t;
  explicit random_generator(size_t seed) : state(seed) { }
  random_generator() : state(0) { }
  void seed(result_type value = 0) { state = value; }
  result_type operator()() { return state = hash64(state); }
  static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
  static constexpr result_type min() { return std::numeric_limits<result_type>::lowest(); }
  random_generator operator[](size_t i) const {
    return random_generator(static_cast<size_t>(hash64((i+1)*0x7fffffff + state))); }
 private:
  result_type state = 0;
};

struct random {
 public:
  random(size_t seed) : state(seed){};
  random() : state(0){};
  random fork(uint64_t i) const { return random(static_cast<size_t>(hash64(hash64(i + state)))); }
  random next() const { return fork(0); }
  size_t ith_rand(uint64_t i) const { return static_cast<size_t>(hash64(i + state)); }
  size_t operator[](size_t i) const { return ith_rand(i); }
  size_t rand() { return ith_rand(0); }
  size_t max() { return std::numeric_limits<size_t>::max(); }
 private:
  size_t state = 0;
};

namespace internal {

// inplace sequential version
template <typename Iterator>
void seq_random_shuffle_(slice<Iterator, Iterator> A, random r = random()) {
  // the Knuth shuffle
  size_t n = A.size();
  if (n < 2) return;
  for (size_t i=n-1; i > 0; i--) {
    using std::swap;
    swap(A[i], A[r.ith_rand(i) % (i + 1)]);
  }
}

template <typename InIterator, typename OutIterator>
void random_shuffle_(slice<InIterator, InIterator> In,
                     slice<OutIterator, OutIterator> Out,
                     random r = random()) {
                       
  size_t n = In.size();
  if (n < SEQ_THRESHOLD) {
    parallel_for(0,n,[&] (size_t i) {
      assign_uninitialized(Out[i], In[i]);
    });
    seq_random_shuffle_(Out, r);
    return;
  }

  size_t bits = 10;
  if (n < (1 << 27)) {
    bits = (log2_up(n) - 7)/2;
  }
  else {
    bits = (log2_up(n) - 17);
  }
  
  size_t num_buckets = (size_t{1} << bits);
  size_t mask = num_buckets - 1;
  auto rand_pos = [&] (size_t i) -> size_t {
    return r.ith_rand(i) & mask;
  };

  auto get_pos = delayed_seq<size_t>(n, rand_pos);

  // first randomly sorts based on random values [0,num_buckets)
  sequence<size_t> bucket_offsets;
  bool single;
  std::tie(bucket_offsets, single) = count_sort<uninitialized_copy_tag>(
        make_slice(In), make_slice(Out), make_slice(get_pos), num_buckets);

  // now sequentially randomly shuffle within each bucket
  auto bucket_f = [&] (size_t i) {
    size_t start = bucket_offsets[i];
    size_t end = bucket_offsets[i+1];
    seq_random_shuffle_(make_slice(Out).cut(start,end), r.fork(i));
  };
  parallel_for(0, num_buckets, bucket_f, 1);
}

}  // namespace internal

// Generate a random permutation of the given range
// Note that unless a seeded RNG is provided, the
// same permutation will be generated every time by
// the default seed
template <PARLAY_RANGE_TYPE R>
auto random_shuffle(const R& In, random r = random()) {
  using T = range_value_type_t<R>;
  auto Out = sequence<T>::uninitialized(In.size());
  internal::random_shuffle_(make_slice(In), make_slice(Out), r);
  return Out;
}

// Generate a random permutation of the integers from 0 to n - 1
// Note that unless a seeded RNG is provided, the
// same permutation will be generated every time by
// the default seed
template <typename Integer_>
sequence<Integer_> random_permutation(Integer_ n, random r = random()) {
  auto id = sequence<Integer_>::from_function(n,
    [&] (Integer_ i) -> Integer_ { return i; });
  return random_shuffle(id, r);
}

}  // namespace parlay

#endif  // PARLAY_RANDOM_H_
