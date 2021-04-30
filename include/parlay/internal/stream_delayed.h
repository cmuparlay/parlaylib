#include <cstddef>
#include <iterator>

#include "../sequence.h"
#include "../utilities.h"

#include "uninitialized_sequence.h"

namespace parlay {
namespace stream_delayed {

// decorates a minimal iterator Iter that just needs to support:
//    ++iter, *iter, and value_type
// to make into a more full fledged forward range type,
// including a forward iterator, and corresponding sentinal.
template <typename Iter>
struct forward_delayed_sequence {
   public:
  using value_type = typename Iter::value_type;
  struct sentinal;

  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename Iter::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = value_type;

    Iter ii;
    size_t count;

    iterator(Iter ii, size_t n) : ii(ii), count(n) {}
    iterator& operator++() {
      ++ii;
      count--;
      return *this; }
    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp; }
    value_type operator*() const { return *ii; }
    bool operator!=([[maybe_unused]] const sentinal& other) const {
      return count != 0; }
    bool operator==([[maybe_unused]] const sentinal& other) const {
      return count == 0; }
  };

  struct sentinal {
    size_t operator-(const iterator& i) const {
      return i.count; }
    size_t operator-(iterator& i) {
      return i.count; }
    bool operator!=(const iterator& other) const {
      return other.count != 0; }
    bool operator==(const iterator& other) const {
      return other.count == 0; }
  };

  const iterator begin_;
  iterator begin() const {return begin_;}
  sentinal end() const {return sentinal();}
  size_t size() const {return begin_.count;}
  forward_delayed_sequence(Iter ii, size_t n)
      : begin_(iterator(ii, n)) {}
};

template <typename Seq1, typename Seq2, typename F>
auto zip_with(Seq1 &S1, Seq2 &S2, F f) {
  struct iter {
    using iter1_t = decltype(S1.begin());
    using iter2_t = decltype(S2.begin());
    using value_type = decltype(f(*(S1.begin()),*(S2.begin())));
    F g;
    iter1_t iter1;
    iter2_t iter2;
    iter& operator++() {++iter1; ++iter2; return *this;}
    value_type operator*() const {return g(*iter1, *iter2);}
    iter(F g, iter1_t iter1, iter2_t iter2) : g(g), iter1(iter1), iter2(iter2) {}
  };
  return forward_delayed_sequence(iter(f, S1.begin(),S2.begin()), S1.end()-S1.begin());
}

template <typename Seq1, typename Seq2>
auto zip(Seq1 &S1, Seq2 &S2) {
  auto f = [] (auto a, auto b) {return std::pair(a,b);};
  return zip_with(S1, S2, f);
}

template <typename Seq, typename F>
auto map(Seq &S, F f) {
  struct iter {
    using iter_t = decltype(S.begin());
    using value_type = decltype(f(*(S.begin())));
    F g;
    iter_t input_iter;
    iter& operator++() {++input_iter; return *this;}
    value_type operator*() const {return g(*input_iter);}
    iter(F g, iter_t input_iter) : g(g), input_iter(input_iter) {}
  };
  return forward_delayed_sequence(iter(f, S.begin()), S.end()-S.begin());
}

template <typename T, typename F, typename Seq>
auto scan(F f, T const &init, const Seq &S, bool inclusive=false) {
  using input_iter_t = decltype(S.begin());
  struct iter {
    using value_type = T;
    F f;
    value_type value;
    input_iter_t input_iterator;

    iter& operator++() {
      value = f(value, *input_iterator);
      ++input_iterator;
      return *this; }
    T operator*() const { return value; }
    iter(F const &f, T const& init, input_iter_t input_iterator) :
        f(f), value(init), input_iterator(input_iterator) {}
  };
  size_t size = S.end() - S.begin();
  if (inclusive) {
    auto start = S.begin();
    auto next = f(init,*start);
    return forward_delayed_sequence(iter(f, next, ++start), size);
  }
  return forward_delayed_sequence(iter(f, init, S.begin()), size);
}

template <typename T, typename F, typename Seq>
T reduce(F f, T const &init, const Seq &a) {
  auto result = init;
  for (auto s = a.begin(); s != a.end(); ++s)
    result = f(result,*s);
  return result;
}

template <typename Seq, typename F>
void apply(const Seq &a, F f) {
  for (auto s = a.begin(); s != a.end(); ++s) f(*s);
}

template <typename Seq1, typename Seq2, typename F>
void zip_apply(const Seq1 &s1, const Seq2 &s2, F f) {
  for (auto i1 = s1.begin(), i2 = s2.begin(); i1 != s1.end(); ++i1, ++i2)
    f(*i1,*i2);
}


// allocates own temporary space, returns just filtered sequence
template <typename SeqIn, typename F, typename G>
auto filter_map(const SeqIn &In, F f, G g) {
  size_t n = In.size();
  using T = decltype(g(*(In.begin())));
  auto tmp_out = parlay::internal::uninitialized_sequence<T>(n);
  auto out_iter = tmp_out.begin();
  for (auto s = In.begin(); s != In.end(); ++s) {
    auto val = *s;
    if (f(val)) {
      *out_iter = g(val);
      ++out_iter;
    }
  }
  size_t m = out_iter - tmp_out.begin();
  auto result = parlay::sequence<T>::uninitialized(m);
  for (size_t i=0; i < m; i++)
    assign_uninitialized(result[i], std::move(tmp_out[i]));
  return result;
}

}  // namespace stream_delayed
}  // namespace parlay
