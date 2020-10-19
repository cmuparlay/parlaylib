
#ifndef PARLAY_MERGE_H_
#define PARLAY_MERGE_H_

#include "binary_search.h"

#include "../sequence.h"
#include "../utilities.h"

namespace parlay {
namespace internal {

// the following parameter can be tuned
#ifdef PAR_GRANULARITY
constexpr const size_t _merge_base = PAR_GRANULARITY;
#else
constexpr const size_t _merge_base = 2000;
#endif

template <typename assignment_tag, typename InIterator1, typename InIterator2, typename OutIterator, typename BinaryOp>
void seq_merge(slice<InIterator1, InIterator1> A,
               slice<InIterator2, InIterator2> B,
               slice<OutIterator, OutIterator> R,
               const BinaryOp &f) {
  size_t nA = A.size();
  size_t nB = B.size();
  size_t i = 0;
  size_t j = 0;
  while (true) {
    if (i == nA) {
      while (j < nB) {
        assign_dispatch(R[i + j], B[j], assignment_tag{});
        j++;
      }
      break;
    }
    if (j == nB) {
      while (i < nA) {
        assign_dispatch(R[i + j], A[i], assignment_tag{});
        i++;
      }
      break;
    }
    if (f(B[j], A[i])) {
      assign_dispatch(R[i + j], B[j], assignment_tag{});
      j++;
    } else {
      assign_dispatch(R[i + j], A[i], assignment_tag{});
      i++;
    }
  }
}


template <typename assignment_tag, typename InIterator1, typename InIterator2, typename OutIterator, typename BinaryOp>
void merge_into(slice<InIterator1, InIterator1> A,
            slice<InIterator2, InIterator2> B,
            slice<OutIterator, OutIterator> R,
            const BinaryOp &f,
            bool cons = false) {
  size_t nA = A.size();
  size_t nB = B.size();
  size_t nR = nA + nB;
  if (nR < _merge_base) {
    seq_merge<assignment_tag>(A, B, R, f);
  }
  else if (nA == 0) {
    parallel_for(0, nB, [&](size_t i) {
      assign_dispatch(R[i], B[i], assignment_tag{});
    });
  }
  else if (nB == 0) {
    parallel_for(0, nA, [&](size_t i) {
      assign_dispatch(R[i], A[i], assignment_tag{});
    });
  }
  else {
    size_t mA = nA / 2;
    // important for stability that binary search identifies
    // first element in B greater or equal to A[mA]
    size_t mB = binary_search(B, A[mA], f);
    if (mB == 0) mA++;  // ensures at least one on each side
    size_t mR = mA + mB;
    auto left = [&]() {
      merge_into<assignment_tag>(A.cut(0, mA), B.cut(0, mB), R.cut(0, mR), f, cons);
    };
    auto right = [&]() {
      merge_into<assignment_tag>(A.cut(mA, nA), B.cut(mB, nB), R.cut(mR, nR), f, cons);
    };
    par_do(left, right, cons);
  }
}

// Merge A and B, copying their contents
// into the resulting sequence.
template <typename IteratorA, typename IteratorB, typename BinaryOp>
auto merge(slice<IteratorA, IteratorA> A,
           slice<IteratorB, IteratorB> B,
           const BinaryOp &f,
           bool cons = false) {
    
  using T = typename slice<IteratorA, IteratorA>::value_type;
  auto R = sequence<T>::uninitialized(A.size() + B.size());
  merge_into<uninitialized_copy_tag>(A, B, make_slice(R), f, cons);
  return R;
}

// Merge A and B, moving their contents
// into the resulting sequence
template <typename IteratorA, typename IteratorB, typename BinaryOp>
auto merge_move(slice<IteratorA, IteratorA> A,
                slice<IteratorB, IteratorB> B,
                const BinaryOp &f,
                bool cons = false) {
    
  using T = typename slice<IteratorA, IteratorA>::value_type;
  auto R = sequence<T>::uninitialized(A.size() + B.size());
  merge_into<uninitialized_move_tag>(A, B, make_slice(R), f, cons);
  return R;
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_MERGE_H_
