
#ifndef PARLAY_MERGE_H_
#define PARLAY_MERGE_H_

#include "binary_search.h"
#include "sequence.h"
#include "utilities.h"

namespace parlay {
namespace internal {
// TODO: not yet optimized to use moves instead of copies.

// the following parameter can be tuned
#ifdef PAR_GRANULARITY
constexpr const size_t _merge_base = PAR_GRANULARITY;
#else
constexpr const size_t _merge_base = 2000;
#endif

template <_copy_type ct, typename SeqA, typename SeqB, typename Iterator, typename F>
void seq_merge(SeqA const &A, SeqB const &B,
               slice<Iterator, Iterator> R, const F &f) {
  size_t nA = A.size();
  size_t nB = B.size();
  size_t i = 0;
  size_t j = 0;
  while (true) {
    if (i == nA) {
      while (j < nB) {
        copy_val<ct>(R[i + j], B[j]);
        j++;
      }
      break;
    }
    if (j == nB) {
      while (i < nA) {
        copy_val<ct>(R[i + j], A[i]);
        i++;
      }
      break;
    }
    if (f(B[j], A[i])) {
      copy_val<ct>(R[i + j], B[j]);
      j++;
    } else {
      copy_val<ct>(R[i + j], A[i]);
      i++;
    }
  }
}

// this merge is stable
template <_copy_type ct, typename InIterator1, typename InIterator2, typename OutIterator, typename F>
void merge_(const slice<InIterator1, InIterator1> A,
            const slice<InIterator2, InIterator2> B,
            slice<OutIterator, OutIterator> R,
            const F &f, bool cons = false) {
  size_t nA = A.size();
  size_t nB = B.size();
  size_t nR = nA + nB;
  if (nR < _merge_base)
    seq_merge<ct>(A, B, R, f);
  else if (nA == 0)
    parallel_for(0, nB, [&](size_t i) { copy_val<ct>(R[i], B[i]); });
  else if (nB == 0)
    parallel_for(0, nA, [&](size_t i) { copy_val<ct>(R[i], A[i]); });
  else {
    size_t mA = nA / 2;
    // important for stability that binary search identifies
    // first element in B greater or equal to A[mA]
    size_t mB = binary_search(B, A[mA], f);
    if (mB == 0) mA++;  // ensures at least one on each side
    size_t mR = mA + mB;
    auto left = [&]() {
      merge_<ct>(A.cut(0, mA), B.cut(0, mB), R.cut(0, mR), f, cons);
    };
    auto right = [&]() {
      merge_<ct>(A.cut(mA, nA), B.cut(mB, nB), R.cut(mR, nR), f, cons);
    };
    par_do(left, right, cons);
  }
}

template <class SeqA, class SeqB, class F>
sequence<typename SeqA::value_type> merge(const SeqA &A, const SeqB &B,
                                          const F &f, bool cons = false) {
  using T = typename SeqA::value_type;
  auto R = sequence<T>::uninitialized(A.size() + B.size());
  merge_<_assign>(A, B, make_slice(R), f, cons);
  return R;
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_MERGE_H_
