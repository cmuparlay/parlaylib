// Terminal operations on block-iterable sequences are those that convert
// a block-iterable sequence into a non-block iterable output, such as
// reduce, which sums each element in the sequence to a single element,
// or to_sequence, which produces a regular (non-delayed) sequence

#ifndef PARLAY_INTERNAL_DELAYED_TERMINAL_H
#define PARLAY_INTERNAL_DELAYED_TERMINAL_H

#include <cstddef>

#include <functional>
#include <utility>
#include <type_traits>

#include "../../monoid.h"
#include "../../parallel.h"
#include "../../range.h"
#include "../../sequence.h"
#include "../../slice.h"

#include "../sequence_ops.h"

#include "common.h"

namespace parlay {
namespace internal {
namespace delayed {

// ----------------------------------------------------------------------------
//            Conversion of delayed sequences to regular sequences
// ----------------------------------------------------------------------------

template<typename Range,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto to_sequence(Range&& v) {
  return parlay::to_sequence(std::forward<Range>(v));
}

template<typename T, typename Alloc = parlay::internal::sequence_default_allocator<T>, typename Range,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto to_sequence(Range&& v) {
  return parlay::to_sequence<T, Alloc>(std::forward<Range>(v));
}

template<typename Range,
  std::enable_if_t<!is_random_access_range_v<Range> &&
                    is_block_iterable_range_v<Range>, int> = 0>
auto to_sequence(Range&& v) {
  auto sz = parlay::size(v);
  auto out = parlay::sequence<range_value_type_t<Range>>::uninitialized(sz);
  parallel_for(0, num_blocks(v), [&](size_t i) {
    std::uninitialized_copy(begin_block(v, i), end_block(v, i), out.begin() + i * block_size);
  });
  return out;
}

template<typename T, typename Alloc = parlay::internal::sequence_default_allocator<T>, typename Range,
    std::enable_if_t<!is_random_access_range_v<Range> &&
                      is_block_iterable_range_v<Range>, int> = 0>
auto to_sequence(Range&& v) {
  auto sz = parlay::size(v);
  auto out = parlay::sequence<T, Alloc>::uninitialized(sz);
  parallel_for(0, num_blocks(v), [&](size_t i) {
    std::uninitialized_copy(begin_block(v, i), end_block(v, i), out.begin() + i * block_size);
  });
  return out;
}

// ----------------------------------------------------------------------------
//                                  Reduce
// ----------------------------------------------------------------------------

template<typename Range, typename BinaryOperator, typename T,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto reduce(Range&& v, BinaryOperator f, T identity) {
  static_assert(is_binary_operator_for_v<BinaryOperator, T, range_reference_type_t<Range>>);
  return parlay::internal::reduce(make_slice(v), parlay::binary_op(std::move(f), std::move(identity)));
}

template<typename Range, typename Monoid,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto reduce(Range&& v, Monoid&& m) {
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<Range>>);
  return parlay::internal::reduce(make_slice(v), std::forward<Monoid>(m));
}

template<typename Range,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto reduce(Range&& v) {
  static_assert(is_binary_operator_for_v<std::plus<>, range_value_type_t<Range>, range_reference_type_t<Range>>);
  return reduce(std::forward<Range>(v), std::plus<>{}, range_value_type_t<Range>{});
}

template<typename Range, typename BinaryOperator, typename T,
  std::enable_if_t<!is_random_access_range_v<Range> &&
                    is_block_iterable_range_v<Range>, int> = 0>
auto reduce(Range&& v, BinaryOperator&& f, T identity) {
  static_assert(is_binary_operator_for_v<BinaryOperator, T, range_reference_type_t<Range>>);

  auto block_sums = parlay::internal::tabulate(num_blocks(v), [&](size_t i) {
    T result = identity;
    auto it = begin_block(v, i), last = end_block(v, i);
    for (; it != last; ++it) {
      result = f(std::move(result), *it);
    }
    return result;
  });
  return parlay::internal::reduce(make_slice(block_sums),
            parlay::make_monoid(std::forward<BinaryOperator>(f), std::move(identity)));
}

template<typename Range, typename Monoid,
  std::enable_if_t<!is_random_access_range_v<Range> &&
                    is_block_iterable_range_v<Range>, int> = 0>
auto reduce(Range&& v, Monoid&& m) {
  static_assert(is_monoid_for_v<Monoid, range_reference_type_t<Range>>);
  auto identity = m.identity;
  return reduce(std::forward<Range>(v), std::forward<Monoid>(m), std::move(identity));
}

template<typename Range,
  std::enable_if_t<!is_random_access_range_v<Range> &&
                    is_block_iterable_range_v<Range>, int> = 0>
auto reduce(Range&& v) {
  static_assert(is_binary_operator_for_v<std::plus<>, range_value_type_t<Range>, range_reference_type_t<Range>>);
  return reduce(std::forward<Range>(v), std::plus<>{}, range_value_type_t<Range>{});
}

// ----------------------------------------------------------------------------
//                              For each / apply
// ----------------------------------------------------------------------------

template<typename Range,  typename UnaryFunction,
  std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
void for_each(Range&& v, UnaryFunction&& f) {
  static_assert(std::is_invocable_v<UnaryFunction, range_reference_type_t<Range>>);
  parallel_for(0, size(v), [it = std::begin(v), &f](size_t i) {
    f(it[i]);
  });
}

template<typename Range, typename UnaryFunction,
  std::enable_if_t<!is_random_access_range_v<Range> &&
                    is_block_iterable_range_v<Range>, int> = 0>
void for_each(Range&& v, UnaryFunction&& f) {
  static_assert(std::is_invocable_v<UnaryFunction, range_reference_type_t<Range>>);
  parallel_for(0, num_blocks(v), [&](size_t i) {
    auto it = begin_block(v, i), last = end_block(v, i);
    for (; it != last; ++it) {
      f(*it);
    }
  });
}

template<typename Range, typename UnaryFunction>
void apply(Range&& v, UnaryFunction&& f) {
  static_assert(is_block_iterable_range_v<Range>);
  static_assert(std::is_invocable_v<UnaryFunction, range_reference_type_t<Range>>);
  parlay::internal::delayed::for_each(std::forward<Range>(v), std::forward<UnaryFunction>(f));
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_TERMINAL_H
