// Implementations of many C++ standard library algorithms

#ifndef PARLAY_STLALGS_H_
#define PARLAY_STLALGS_H_

#include "binary_search.h"
#include "kth_smallest.h"
#include "sample_sort.h"
#include "sequence_ops.h"

namespace parlay {

template <class IntegerPred>
size_t count_if_index(size_t n, IntegerPred p) {
  auto BS =
      parlay::delayed_seq<size_t>(n, [&](size_t i) -> size_t { return p(i); });
  size_t r = parlay::reduce(BS, parlay::addm<size_t>());
  return r;
}

template <class IntegerPred>
size_t find_if_index(size_t n, IntegerPred p, size_t granularity = 1000) {
  size_t i;
  for (i = 0; i < std::min(granularity, n); i++)
    if (p(i)) return i;
  if (i == n) return n;
  size_t start = granularity;
  size_t block_size = 2 * granularity;
  i = n;
  while (start < n) {
    size_t end = std::min(n, start + block_size);
    parallel_for(start, end,
                 [&](size_t j) {
                   if (p(j)) write_min(&i, j, std::less<size_t>());
                 },
                 granularity);
    if (i < n) return i;
    start += block_size;
    block_size *= 2;
  }
  return n;
}

template <class Seq, class UnaryFunction>
void for_each(Seq const &S, UnaryFunction f) {
  parallel_for(0, S.size(), [&](size_t i) { f(S[i]); });
}

template <class Seq, class UnaryFunction>
void for_each(Seq &S, UnaryFunction f) {
  parallel_for(0, S.size(), [&](size_t i) { f(S[i]); });
}

template <class Seq, class UnaryPred>
size_t count_if(Seq const &S, UnaryPred p) {
  return count_if_index(S.size(), [&](size_t i) { return p(S[i]); });
}

template <class Seq, class T>
size_t count(Seq const &S, T const &value) {
  return count_if_index(S.size(), [&](size_t i) { return S[i] == value; });
}

template <class Seq, class UnaryPred>
bool all_of(Seq const &S, UnaryPred p) {
  return count_if(S, p) == S.size();
}

template <class Seq, class UnaryPred>
bool any_of(Seq const &S, UnaryPred p) {
  return count_if(S, p) > 1;
}

template <class Seq, class UnaryPred>
bool none_of(Seq const &S, UnaryPred p) {
  return count_if(S, p) == 0;
}

template <class Seq, class UnaryPred>
size_t find_if(Seq const &S, UnaryPred p) {
  return find_if_index(S.size(), [&](size_t i) { return p(S[i]); });
}

template <class Seq, class T>
size_t find(Seq const &S, T const &value) {
  return find_if(S, [&](T x) { return x == value; });
}

template <class Seq, class UnaryPred>
size_t find_if_not(Seq const &S, UnaryPred p) {
  return find_if_index(S.size(), [&](size_t i) { return !p(S[i]); });
}

template <class Seq1, class Seq2, class BinaryPred>
size_t find_first_of(Seq1 const &S1, Seq2 const &S2, BinaryPred p) {
  return find_if_index(S1.size(), [&] (size_t i) {
    size_t j;
    for (j = 0; j < S2.size(); j++)
      if (p(S1[i], S2[j])) break;
    return (j < S2.size());
   });
}

template <class Seq, class BinaryPred>
size_t adjacent_find(Seq const &S, BinaryPred pred) {
  return find_if_index(S.size() - 1,
                       [&](size_t i) { return pred(S[i], S[i + 1]); });
}

template <class Seq>
size_t adjacent_find(Seq const &S) {
  return find_if_index(S.size() - 1,
                       [&](size_t i) { return S[i] == S[i + 1]; });
}

template <class Seq>
size_t mismatch(Seq const &S1, Seq const &S2) {
  return find_if_index(std::min(S1.size(), S2.size()),
                       [&](size_t i) { return S1[i] != S2[i]; });
}

template <class Seq, class BinaryPred>
size_t mismatch(Seq const &S1, Seq const &S2, BinaryPred pred) {
  return find_if_index(std::min(S1.size(), S2.size()),
                       [&](size_t i) { return !pred(S1[i], S2[i]); });
}

template <class Seq1, class Seq2, class BinaryPred>
size_t search(Seq1 const &S1, Seq2 const &S2, BinaryPred pred) {
  return find_if_index(S1.size(), [&](size_t i) {
    if (i + S2.size() > S1.size()) return false;
    size_t j;
    for (j = 0; j < S2.size(); j++)
      if (!pred(S1[i + j], S2[j])) break;
    return (j == S2.size());
  });
}

template <class Seq1, class Seq2>
size_t search(Seq1 const &S1, Seq2 const &S2) {
  using T = typename Seq1::value_type;
  auto eq = [](T a, T b) { return a == b; };
  return parlay::search(S1, S2, eq);
}

template <class Seq, class BinaryPred>
size_t find_end(Seq const &S1, Seq const &S2, BinaryPred pred) {
  size_t n1 = S1.size();
  size_t n2 = S2.size();
  size_t idx = find_if_index(S1.size() - S2.size() + 1, [&](size_t i) {
    size_t j;
    for (j = 0; j < n2; j++)
      if (!pred(S1[(n1 - i - n2) + j], S2[j])) break;
    return (j == S2.size());
  });
  return n1 - idx - n2;
}

template <class Seq>
size_t find_end(Seq const &S1, Seq const &S2) {
  using T = typename Seq::value_type;
  return find_end(S1, S2, [](T a, T b) { return a == b; });
}

template <class Seq1, class Seq2, class BinaryPred>
bool equal(Seq1 const &s1, Seq2 const &s2, BinaryPred p) {
  return count_if_index(s1.size(), [&](size_t i) { return p(s1[i], s2[i]); });
}

template <class Seq1, class Seq2>
bool equal(Seq1 const &s1, Seq2 const &s2) {
  return count_if_index(s1.size(), [&](size_t i) { return s1[i] == s2[i]; });
}

template <class Seq1, class Seq2, class Compare>
bool lexicographical_compare(Seq1 const &s1, Seq2 const &s2, Compare less) {
  size_t m = std::min(s1.size(), s2.size());
  size_t i = find_if_index(
      m, [&](size_t i) { return less(s1[i], s2[i]) || less(s2[i], s1[i]); });
  return (i < m) ? (less(s1[i], s2[i])) : (s1.size() < s2.size());
}

template <class Seq, class Eql>
sequence<typename Seq::value_type> unique(Seq const &s, Eql eq) {
  auto b = delayed_seq<bool>(
      s.size(), [&](size_t i) { return (i == 0) || !eq(s[i], s[i - 1]); });
  return pack(s, b);
}

// needs to return location, and take comparison
template <class Seq, class Compare>
size_t min_element(Seq const &S, Compare comp) {
  auto SS = delayed_seq<size_t>(S.size(), [&](size_t i) { return i; });
  auto f = [&](size_t l, size_t r) { return (!comp(S[r], S[l]) ? l : r); };
  return parlay::reduce(SS, make_monoid(f, (size_t)S.size()));
}

template <class Seq, class Compare>
size_t max_element(Seq const &S, Compare comp) {
  using T = typename Seq::value_type;
  return min_element(S, [&](T a, T b) { return comp(b, a); });
}

template <class Seq, class Compare>
std::pair<size_t, size_t> minmax_element(Seq const &S, Compare comp) {
  size_t n = S.size();
  using P = std::pair<size_t, size_t>;
  auto SS = delayed_seq<P>(S.size(), [&](size_t i) { return P(i, i); });
  auto f = [&](P l, P r) -> P {
    return (P(!comp(S[r.first], S[l.first]) ? l.first : r.first,
              !comp(S[l.second], S[r.second]) ? l.second : r.second));
  };
  return parlay::reduce(SS, make_monoid(f, P(n, n)));
}

/* Operates in-place */
template <class Seq>
auto reverse(Seq &S) {
  auto n = S.size();
  parallel_for(0, n/2, [&] (size_t i) {
    std::swap(S[i], S[n - i - 1]);
  }, 2048);
}

template <class Seq>
sequence<typename Seq::value_type> rotate(Seq const &S, size_t r) {
  size_t n = S.size();
  return sequence<typename Seq::value_type>(S.size(), [&](size_t i) {
    size_t j = (i < r) ? n - r + i : i - r;
    return S[j];
  });
}

template <class Seq, class Compare>
bool is_sorted(Seq const &S, Compare comp) {
  auto B = delayed_seq<bool>(
      S.size() - 1, [&](size_t i) -> size_t { return comp(S[i + 1], S[i]); });
  return (reduce(B, addm<size_t>()) != 0);
}

template <class Seq, class Compare>
size_t is_sorted_until(Seq const &S, Compare comp) {
  return find_if_index(S.size() - 1,
                       [&](size_t i) { return comp(S[i + 1], S[i]); }) +
         1;
}

template <class Seq, class UnaryPred>
size_t is_partitioned(Seq const &S, UnaryPred f) {
  auto B = delayed_seq<bool>(
      S.size() - 1, [&](size_t i) -> size_t { return !f(S[i + 1]) && S[i]; });
  return (reduce(B, addm<size_t>()) != 0);
}

template <class Seq, class UnaryPred>
auto remove_if(Seq const &S, UnaryPred f) {
  auto flags = delayed_seq<bool>(S.size(), [&](auto i) { return f(S[i]); });
  return pack(S, flags);
}

template <class Seq, class Compare>
sequence<typename Seq::value_type> sort(Seq const &S, Compare less) {
  return sample_sort(S, less, false);
}

template <class T, class Compare>
sequence<T> sort(sequence<T> &&S, Compare less) {
  return sample_sort(std::forward<sequence<T>>(S), less, false);
}

template <class Iter, class Compare>
void sort_inplace(range<Iter> A, const Compare &f) {
  sample_sort_inplace(A, f);
};

template <class Seq, class Compare>
sequence<typename Seq::value_type> stable_sort(Seq const &S, Compare less) {
  return sample_sort(S, less, true);
}

template <class Seq, class Compare>
sequence<typename Seq::value_type> remove_duplicates_ordered(Seq const &s,
                                                             Compare less) {
  using T = typename Seq::value_type;
  return unique(stable_sort(s, less),
                [&](T a, T b) { return !less(a, b) && !less(b, a); });
}

template <class Seq1, class Seq2>
sequence<typename Seq1::value_type> append(Seq1 const &s1, Seq2 const &s2) {
  using T = typename Seq1::value_type;
  size_t n1 = s1.size();
  return sequence<T>(n1 + s2.size(),
                     [&](size_t i) { return (i < n1) ? s1[i] : s2[i - n1]; });
}

template <class Index, class BoolSeq>
std::pair<sequence<Index>, Index> enumerate(BoolSeq const &s) {
  return scan(
      delayed_seq<Index>(s.size(), [&](size_t i) -> Index { return s[i]; }),
      addm<Index>());
}

template <class Index>
auto iota(Index n) {
  return dseq(n, [&](size_t i) -> Index { return i; });
}

template <class Seq>
auto flatten(Seq const &s) -> sequence<typename Seq::value_type::value_type> {
  using T = typename Seq::value_type::value_type;
  sequence<size_t> offsets(s.size(), [&](size_t i) { return s[i].size(); });
  size_t len = scan_inplace(offsets.slice(), addm<size_t>());
  auto r = sequence<T>::no_init(len);
  parallel_for(0, s.size(), [&](size_t i) {
    parallel_for(
        0, s[i].size(),
        [&](size_t j) { assign_uninitialized(r[offsets[i] + j], s[i][j]); },
        1000);
  });
  return r;
}

template <typename Seq, typename Monoid, typename UnaryOp>
auto transform_reduce(Seq const &s, Monoid m, UnaryOp unary_op) {
  using T = typename Seq::value_type;
  auto transformed_seq =
      delayed_seq<T>(s.size(), [&](size_t i) { return unary_op(s[i]); });
  return reduce(transformed_seq, m);
}

template <typename Seq, typename Monoid, typename UnaryOp>
auto transform_exclusive_scan(Seq const &s, Monoid m, UnaryOp unary_op) {
  using T = typename Seq::value_type;
  auto transformed_seq =
      delayed_seq<T>(s.size(), [&](size_t i) { return unary_op(s[i]); });
  return scan(transformed_seq, m);
}

}  // namespace parlay

#endif  // PARLAY_STLALGS_H_
