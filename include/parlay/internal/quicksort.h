
#ifndef PARLAY_QUICKSORT_H_
#define PARLAY_QUICKSORT_H_

#include <algorithm>
#include <new>
#include <utility>
#include <cassert>

#include "uninitialized_storage.h"
#include "uninitialized_sequence.h"
#include "sequence_ops.h"
#include "counting_sort.h"
#include "sequence_ops.h"

#include "../utilities.h"

namespace parlay {
namespace internal {

template <typename Iterator>
bool base_case(Iterator x, size_t n) {
  using value_type = typename std::iterator_traits<Iterator>::value_type;
  bool large = std::is_pointer<value_type>::value || (sizeof(x) > 8);
  return large ? (n < 16) : (n < 24);
}

// Optimized insertion sort using uninitialized relocate
//
// Note that the strange looking loop body is really just a
// slightly more optimized version of:
//
//    T tmp = std::move(A[i])
//    std::ptrdiff_t j = i - 1;
//    for (; j >= 0 && f(tmp, A[j]); j--) {
//      A[j+1] = std::move(A[j])
//    }
//    A[j+1] = std::move(tmp);
//
// but instead of using move, we use relocation to potentially
// save on wasteful destructor invocations.
//
// template <class Iterator, typename BinPred>
// auto insertion_sort(Iterator A, size_t n, const BinPred& f) {
//   using T = typename std::iterator_traits<Iterator>::value_type;
//   for (size_t i = 1; i < n; i++) {
//     // We want to store the value being moved (A[i]) in a temporary
//     // variable. In order to take advantage of uninitialized
//     // relocate, we need uninitialized storage to put it in.
//     uninitialized_storage<T> temp_storage;
//     T* tmp = temp_storage.get();
//     uninitialized_relocate(tmp, &A[i]);

//     std::ptrdiff_t j = i - 1;
//     for (; j >= 0 && f(*tmp, A[j]); j--) {
//       uninitialized_relocate(&A[j+1], &A[j]);
//     }
//     uninitialized_relocate(&A[j+1], tmp);
//   }
// }

template <class Iterator, class BinPred>
void insertion_sort(Iterator A, size_t n, const BinPred& f) {
  for (size_t i = 1; i < n; i++) {
    long j = i;
    while (--j >= 0 && f(A[j + 1], A[j])) {
      using std::swap;
      swap(A[j + 1], A[j]);
    }
  }
}

// Sorts 5 sampled elements and puts them at the front.
template <class Iterator, class BinPred>
void sort5(Iterator A, size_t n, const BinPred& f) {
  size_t size = 5;
  for (size_t i = 0; i < size; ++i) {
    size_t j = i + parlay::hash64(i) % (n - i);
    using std::swap;  // enable ADL.
    swap(A[i], A[j]);
  }
  insertion_sort(A, size, f);
}

// Dual-pivot partition. Picks two pivots from the input A
// and then divides it into three parts:
//
//   [x < p1), [p1 <= x <= p2], (p2 < x]
//
// Returns a triple consisting of iterators to the start
// of the second and third part, and a boolean flag, which
// is true if the pivots were equal, and hence the middle
// part contains all equal elements.
//
// Requires that the size of A is at least 5.
template <class Iterator, class BinPred>
std::tuple<Iterator, Iterator, bool> split3(Iterator A, size_t n, const BinPred& f) {
  assert(n >= 5);
  sort5(A, n, f);
  
  // Use A[1] and A[3] as the pivots. Move them to
  // the front so that A[0] and A[1] are the pivots
  using std::swap;
  swap(A[0], A[1]);
  swap(A[1], A[3]);
  const auto& p1 = A[0];
  const auto& p2 = A[1];
  bool pivots_equal = !f(p1, p2);
  
  // set up initial invariants
  auto L = A + 2;
  auto R = A + n - 1;
  while (f(*L, p1)) L++;
  while (f(p2, *R)) R--;
  auto M = L;
  
  // invariants:
  //  below L is less than p1,
  //  above R is greater than p2
  //  between L and M are between p1 and p2 inclusive
  //  between M and R are unprocessed
  while (M <= R) {

    if (f(*M, p1)) {
      swap(*M, *L);
      L++;
    } else if (f(p2, *M)) {
      swap(*M, *R);
      if (f(*M, p1)) {
        swap(*L, *M);
        L++;
      }

      R--;
      while (f(p2, *R)) R--;
    }
    M++;
  }

  // Swap the pivots into position
  L -= 2;
  swap(A[1], *(L+1));
  swap(A[0], *L);
  swap(*(L+1), *R);

  return std::make_tuple(L, M, pivots_equal);
}

template <class Iterator, class BinPred>
void quicksort_serial(Iterator A, size_t n, const BinPred& f) {
  while (!base_case(A, n)) {
    auto [L, M, mid_eq] = split3(A, n, f);
    if (!mid_eq) quicksort_serial(L+1, M - L-1, f);
    quicksort_serial(M, A + n - M, f);
    n = L - A;
  }

  insertion_sort(A, n, f);
}

template <class Iterator, class BinPred>
void quicksort(Iterator A, size_t n, const BinPred& f) {
  if (n < (1 << 8))
    quicksort_serial(A, n, f);
  else {
    auto [L, M, mid_eq] = split3(A, n, f);
    // Note: generic lambda capture for L and M (i.e. writing L = L, etc.)
    // in the capture is required due to a defect in the C++ standard which
    // prohibits capturing names resulting from a structured binding!
    auto left = [&, L = L]() { quicksort(A, L - A, f); };
    auto mid = [&, L = L, M = M]() { quicksort(L + 1, M - L - 1, f); };
    auto right = [&, M = M]() { quicksort(M, A + n - M, f); };

    if (!mid_eq)
      par_do3(left, mid, right);
    else
      par_do(left, right);
  }
}

template <class Iterator, class BinPred>
void quicksort(slice<Iterator, Iterator> A, const BinPred& f) {
  quicksort(A.begin(), A.size(), f);
}

//// Fully Parallel version below here

// ---------------  Not currently tested or used ----------------

template <class assignment_tag, class Iterator, class BinPred>
std::tuple<size_t, size_t, bool> p_split3(slice<Iterator, Iterator> A,
                                          slice<Iterator, Iterator> B,
                                          const BinPred& f) {
  using E = typename std::iterator_traits<Iterator>::value_type;
  size_t n = A.size();
  sort5(A.begin(), n, f);
  E p1 = A[1];
  E p2 = A[3];
  if (!f(A[0], A[1])) p1 = p2;  // if few elements less than p1, then set to p2
  if (!f(A[3], A[4]))
    p2 = p1;  // if few elements greater than p2, then set to p1
  if (true) {
    auto flag = [&](size_t i) { return f(A[i], p1) ? 0 : f(p2, A[i]) ? 2 : 1; };
    auto r = split_three<assignment_tag>(A, make_slice(B),
					 make_slice(delayed_seq<unsigned char>(n, flag)),
					 fl_conservative);
    return std::make_tuple(r.first, r.first + r.second, !f(p1, p2));
  } else { // currently not used 
    auto buckets = dseq(n, [&] (size_t i) {
	    return f(A[i], p1) ? 0 : f(p2, A[i]) ? 2 : 1; });
    auto r = count_sort<assignment_tag>(A, B, make_slice(buckets), 3, .9);
    return std::make_tuple(r.first[0], r.first[0] + r.first[1], !f(p1, p2));
  } 
}

// The fully parallel version copies back and forth between two arrays
// inplace: if true then result is put back in In
//     and Out is just used as temp space
//     otherwise result is in Out
//     In and Out cannot be the same (Out is needed as temp space)
// cut_size: is when to revert to  quicksort.
//    If -1 then it uses a default based on number of threads
template <typename assignment_tag, typename InIterator, typename OutIterator, typename F>
void p_quicksort_(slice<InIterator, InIterator> In,
                  slice<OutIterator, OutIterator> Out,
                  const F& f,
                  bool inplace = false,
                  long cut_size = -1) {
  size_t n = In.size();
  if (cut_size == -1)
    cut_size = std::max<long>((3 * n) / num_workers(), (1 << 14));
  if (n < (size_t)cut_size) {
    quicksort(In.begin(), n, f);
    if (!inplace)
      parallel_for(0, n, [&] (size_t i) {
         assign_dispatch(Out[i], In[i], assignment_tag()); }, 2000);
  } else {
    size_t l, m;
    bool mid_eq;
    std::tie(l, m, mid_eq) = p_split3<assignment_tag>(In, Out, f);
    par_do3(
        [&]() {
          p_quicksort_<assignment_tag>(Out.cut(0, l), In.cut(0, l), f, !inplace, cut_size);
        },
        [&]() {
          auto copy_in = [&](size_t i) { In[i] = Out[i]; };
          if (!mid_eq)
            p_quicksort_<assignment_tag>(Out.cut(l, m), In.cut(l, m), f, !inplace,
                         cut_size);
          else if (inplace)
            parallel_for(l, m, copy_in, 2000);
        },
        [&]() {
          p_quicksort_<assignment_tag>(Out.cut(m, n), In.cut(m, n), f, !inplace, cut_size);
        });
  }
}

template <typename Iterator, class F>
void p_quicksort_inplace(slice<Iterator, Iterator> In, const F& f) {
  using value_type = typename slice<Iterator, Iterator>::value_type;

  auto Tmp = uninitialized_sequence<value_type>(In.size());
  p_quicksort_<uninitialized_move_tag>(In, make_slice(Tmp), f, true);
  //p_quicksort_<uninitialized_relocate_tag>(In, make_slice(Tmp), f, true);
}

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_QUICKSORT_H_
