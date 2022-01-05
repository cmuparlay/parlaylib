#ifndef PARLAY_INTERNAL_DELAYED_MAP_H_
#define PARLAY_INTERNAL_DELAYED_MAP_H_

#include <type_traits>

#include "../../range.h"
#include "../../type_traits.h"
#include "../../utilities.h"

#include "../sequence_ops.h"

#include "common.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView, typename UnaryOperator>
struct block_delayed_map_t : public block_iterable_view_base<UnderlyingView, block_delayed_map_t<UnderlyingView, UnaryOperator>> {

  using base = block_iterable_view_base<UnderlyingView, block_delayed_map_t<UnderlyingView, UnaryOperator>>;
  using base::base_view;

  using underlying_block_iterator_type = range_block_iterator_type_t<UnderlyingView>;

  using reference = std::invoke_result_t<UnaryOperator, range_reference_type_t<UnderlyingView>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;

  template<typename UV>
  block_delayed_map_t(UV&& v, UnaryOperator f) : base(std::forward<UV>(v)), op(std::move(f)) {}

  struct block_iterator {
    using iterator_category = std::forward_iterator_tag;
    using reference = reference;
    using value_type = value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    decltype(auto) operator*() const { return (*op)(*it); }

    block_iterator& operator++() { ++it; return *this; }
    block_iterator operator++(int) const { block_iterator ip = *this; ++ip; return ip; }

    bool operator==(const block_iterator& other) const { return it == other.it; }
    bool operator!=(const block_iterator& other) const { return it != other.it; }

   private:
    friend struct block_delayed_map_t<UnderlyingView,UnaryOperator>;

    block_iterator() : it{}, op(nullptr) {}
    block_iterator(underlying_block_iterator_type it_, const UnaryOperator* op_) : it(it_), op(op_) {}

    underlying_block_iterator_type it;
    const UnaryOperator* op;
  };

  using const_block_iterator = block_iterator;

  // Returns the number of blocks
  auto get_num_blocks() const { return num_blocks(base_view()); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return block_iterator(begin_block(base_view(), i), op.get()); }
  auto get_begin_block(size_t i) const { return block_iterator(begin_block(base_view(), i), op.get()); }

  [[nodiscard]] size_t size() const { return base_view().size(); }

 private:
  copyable_function_wrapper<UnaryOperator> op;
};

template<typename UnderlyingView, typename UnaryOperator,
    std::enable_if_t<is_random_access_range_v<UnderlyingView>, int> = 0>
auto map(UnderlyingView&& v, UnaryOperator f) {
  return parlay::internal::delayed_map(std::forward<UnderlyingView>(v), std::move(f));
}

template<typename UnderlyingView, typename UnaryOperator,
    std::enable_if_t<!is_random_access_range_v<UnderlyingView> &&
                      is_block_iterable_range_v<UnderlyingView>, int> = 0>
auto map(UnderlyingView&& v, UnaryOperator f) {
  return parlay::internal::delayed::block_delayed_map_t<UnderlyingView, UnaryOperator>
      (std::forward<UnderlyingView>(v), std::move(f));
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_MAP_H_