
#ifndef PARLAY_RANDOM_H_
#define PARLAY_RANDOM_H_

#include <cstddef>
#include <cstdint>

#include <tuple>

#include "delayed_sequence.h"
#include "parallel.h"
#include "range.h"
#include "sequence.h"
#include "slice.h"
#include "utilities.h"

#include "internal/counting_sort.h"

namespace parlay {

// A cheap version of an inteface that should be improved
// Allows forking a state into multiple states
struct random {
 public:
  random(size_t seed) : state(seed){};
  random() : state(0){};
  random fork(uint64_t i) const { return random(static_cast<size_t>(hash64(hash64(i + state)))); }
  random next() const { return fork(0); }
  size_t ith_rand(uint64_t i) const { return static_cast<size_t>(hash64(i + state)); }
  size_t operator[](size_t i) const { return ith_rand(i); }
  size_t rand() { return ith_rand(0); }

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
  for (size_t i=n-1; i > 0; i--)
    std::swap(A[i],A[r.ith_rand(i)%(i+1)]);
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
template <typename _Integer>
sequence<_Integer> random_permutation(_Integer n, random r = random()) {
  auto id = sequence<_Integer>::from_function(n,
    [&] (_Integer i) -> _Integer { return i; });
  return random_shuffle(id, r);
}

}  // namespace parlay

#endif  // PARLAY_RANDOM_H_
