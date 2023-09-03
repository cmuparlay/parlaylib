#ifndef PARLAY_INTERNAL_DELAYED_FILTER_H_
#define PARLAY_INTERNAL_DELAYED_FILTER_H_

#include <cstddef>
#include <utility>

#include "../../range.h"
#include "../../relocation.h"
#include "../../sequence.h"
#include "../../utilities.h"

#include "../sequence_ops.h"
#include "../uninitialized_sequence.h"

#include "common.h"
#include "flatten.h"
#include "map.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView, typename UnaryPredicate>
struct block_delayed_filter_t :
    public block_iterable_view_base<UnderlyingView, block_delayed_filter_t<UnderlyingView, UnaryPredicate>> {

 private:
  using base = block_iterable_view_base<UnderlyingView, block_delayed_filter_t<UnderlyingView, UnaryPredicate>>;
  using base::base_view;

  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<UnderlyingView>>);

  using block_type = sequence<range_iterator_type_t<UnderlyingView>>;
  using block_result_type = sequence<block_type>;
  using flattener_type = block_delayed_flatten_t<block_result_type>;
  using mapper_type = block_delayed_map_t<flattener_type, dereference>;

 public:
  using value_type = range_value_type_t<UnderlyingView>;
  using reference = range_reference_type_t<UnderlyingView>;

  template<typename UV>
  block_delayed_filter_t(UV&& v, UnaryPredicate p_) : base(std::forward<UV>(v), 0), p(std::move(p_)),
      result(flattener_type{filter_blocks(base_view(), p), 0}, dereference{}) { }

  // The default copy constructor is not viable because it might copy dangling iterators
  block_delayed_filter_t(const block_delayed_filter_t& other) : base(other), p(other.p),
    result(flattener_type{filter_blocks(base_view(), p), 0}, dereference{}) { }

  block_delayed_filter_t(block_delayed_filter_t&&) noexcept(
    std::is_nothrow_move_constructible_v<base>                                      &&
    std::is_nothrow_move_constructible_v<copyable_function_wrapper<UnaryPredicate>> &&
    std::is_nothrow_move_constructible_v<mapper_type>) = default;

  friend void swap(block_delayed_filter_t& first, block_delayed_filter_t& second) {
    using std::swap;
    swap(first.base_view(), second.base_view());
    swap(first.result, second.result);
    swap(first.p, second.p);
  }

  block_delayed_filter_t& operator=(block_delayed_filter_t other) {
    swap(*this, other);
    return *this;
  }

  // Returns the number of elements in the filtered range
  [[nodiscard]] size_t size() const { return result.size(); }

  // Returns the number of blocks in the filtered range
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
      return filter_block(begin_block(v, i), end_block(v, i), p, temp_size);
    });
  }

  template<typename It, typename UP>
  auto filter_block(It first, It last, UP&& p, size_t size) {
    parlay::internal::uninitialized_sequence<It> temp(size);
    size_t n = 0;
    for (; first != last; ++first) {
      if (p(*first)) {
        assign_uninitialized(temp[n++], first);
      }
    }
    auto res = sequence<It>::uninitialized(n);
    uninitialized_relocate_n(res.begin(), temp.begin(), n);
    return res;
  }

  copyable_function_wrapper<UnaryPredicate> p;
  mapper_type result;
};

// Given a range v and a predicate p on the reference type of v,
// returns a delayed range that references the elements of v for
// which p returns true.
template<typename UnderlyingView, typename UnaryPredicate>
auto filter(UnderlyingView&& v, UnaryPredicate p) {
  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(std::is_invocable_r_v<bool, UnaryPredicate, range_reference_type_t<UnderlyingView>>);
  return block_delayed_filter_t<UnderlyingView, UnaryPredicate>(
      std::forward<UnderlyingView>(v), std::move(p));
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FILTER_H_