#ifndef PARLAY_INTERNAL_DELAYED_FILTER_OP_H_
#define PARLAY_INTERNAL_DELAYED_FILTER_OP_H_

#include <optional>
#include <type_traits>

#include "../../range.h"
#include "../../type_traits.h"
#include "../../utilities.h"

#include "../sequence_ops.h"

#include "common.h"
#include "flatten.h"
#include "map.h"

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
  block_delayed_filter_op_t(UV&& v, UP&& p) : base(std::forward<UV>(v)),
      result(filter_blocks(base_view(), std::forward<UP>(p)), 0) {

  }

  // Returns the number of blocks
  auto get_num_blocks() const { return result.get_num_blocks(); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return result.get_begin_block(i); }
  auto get_begin_block(size_t i) const { return result.get_begin_block(i); }

  auto get_end_block(size_t i) { return get_begin_block(i+1); }
  auto get_end_block(size_t i) const { return get_begin_block(i+1); }

  auto begin() { return get_begin_block(0); }
  auto begin() const { return get_begin_block(0); }

  auto end() { return get_begin_block(get_num_blocks()); }
  auto end() const { return get_begin_block(get_num_blocks()); }

  [[nodiscard]] size_t size() const { return result.size(); }

 private:
  template<typename UV, typename UP>
  block_result_type filter_blocks(UV&& v, UP&& p) {
    return parlay::internal::tabulate(num_blocks(v), [&](size_t i) -> block_type {
      block_type res;
      for (auto it = begin_block(v, i); it != end_block(v, i); ++it) {
        auto opt = p(*it);
        if (opt.has_value()) {
          res.emplace_back(std::move(*opt));
        }
      }
      return res;
    });
  }

  flattener_type result;
};

template<typename UnderlyingView, typename UnaryOperator>
auto filter_op(UnderlyingView&& v, UnaryOperator&& p) {
  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(std::is_invocable_v<UnaryOperator, range_reference_type_t<UnderlyingView>>);
  return block_delayed_filter_op_t<UnderlyingView, UnaryOperator>(
      std::forward<UnderlyingView>(v), std::forward<UnaryOperator>(p));
}

template<typename UnderlyingView, typename UnaryOperator>
auto map_maybe(UnderlyingView&& v, UnaryOperator&& p) {
  return filter_op(std::forward<UnderlyingView>(v), std::forward<UnaryOperator>(p));
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FILTER_OP_H_