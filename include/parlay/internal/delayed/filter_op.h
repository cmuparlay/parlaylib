#ifndef PARLAY_INTERNAL_DELAYED_FILTER_OP_H_
#define PARLAY_INTERNAL_DELAYED_FILTER_OP_H_

#include <cstddef>

#include <type_traits>
#include <utility>

#include "../../range.h"
#include "../../sequence.h"
#include "../../utilities.h"

#include "../sequence_ops.h"
#include "../uninitialized_sequence.h"

#include "common.h"
#include "flatten.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView, typename UnaryOperator>
struct block_delayed_filter_op_t :
    public block_iterable_view_base<UnderlyingView, block_delayed_filter_op_t<UnderlyingView, UnaryOperator>> {

 private:
  using base = block_iterable_view_base<UnderlyingView, block_delayed_filter_op_t<UnderlyingView, UnaryOperator>>;
  using base::base_view;

  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(std::is_invocable_v<UnaryOperator, range_reference_type_t<UnderlyingView>>);

  using optional_type = std::invoke_result_t<UnaryOperator, range_reference_type_t<UnderlyingView>>;
  static_assert(is_optional_v<optional_type>);
  using result_type = typename optional_type::value_type;
  static_assert(std::is_move_constructible_v<result_type>);

  using block_type = sequence<result_type>;
  using block_result_type = sequence<block_type>;
  using flattener_type = block_delayed_flatten_t<block_result_type>;

 public:
  using value_type = result_type;
  using reference = const result_type&;

  template<typename UV, typename UP>
  block_delayed_filter_op_t(UV&& v, UP&& p) : base(std::forward<UV>(v), 0),
      result(filter_blocks(base_view(), std::forward<UP>(p)), 0) {

  }

  // Returns the number of elements in the resulting range
  [[nodiscard]] size_t size() const { return result.size(); }

  // Returns the number of blocks in the resulting range
  auto get_num_blocks() const { return result.get_num_blocks(); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return result.get_begin_block(i); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) const { return result.get_begin_block(i); }

 private:
  template<typename UV, typename UP>
  auto filter_blocks(UV&& v, UP&& p) {
    size_t temp_size = (std::min<size_t>)(parlay::size(v), block_size);
    return parlay::internal::tabulate(num_blocks(v), [&](size_t i) {
      // Note: For some reason, inlining this code results in a performance
      // decrease!! Calling a separate function here is 2% faster (on GCC 9)
      return filter_block(begin_block(v,i), end_block(v,i), p, temp_size);
    });
  }

  template<typename It, typename UP>
  auto filter_block(It first, It last, UP&& p, size_t size) {
    parlay::internal::uninitialized_sequence<result_type> temp(size);
    size_t n = 0;
    for (; first != last; ++first) {
      auto opt = p(*first);
      if (opt.has_value()) {
        assign_uninitialized(temp[n++], std::move(*opt));
      }
    }
    auto res = sequence<result_type>::uninitialized(n);
    uninitialized_relocate_n(res.begin(), temp.begin(), n);
    return res;
  }

  flattener_type result;
};

// Given a range v and a function p on the reference type of v
// that returns std::optional<T> for some type T, returns a
// delayed range consisting of the values held by the optionals
// produced by p on the values of v such that the optional is
// not empty.
template<typename UnderlyingView, typename UnaryOperator>
auto filter_op(UnderlyingView&& v, UnaryOperator&& p) {
  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(std::is_invocable_v<UnaryOperator, range_reference_type_t<UnderlyingView>>);
  return block_delayed_filter_op_t<UnderlyingView, UnaryOperator>(
      std::forward<UnderlyingView>(v), std::forward<UnaryOperator>(p));
}

// Given a range v and a function p on the reference type of v
// that returns std::optional<T> for some type T, returns a
// delayed range consisting of the values held by the optionals
// produced by p on the values of v such that the optional is
// not empty.
template<typename UnderlyingView, typename UnaryOperator>
auto map_maybe(UnderlyingView&& v, UnaryOperator&& p) {
  return filter_op(std::forward<UnderlyingView>(v), std::forward<UnaryOperator>(p));
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FILTER_OP_H_
