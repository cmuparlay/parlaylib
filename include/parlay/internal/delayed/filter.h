#ifndef PARLAY_INTERNAL_DELAYED_FILTER_H_
#define PARLAY_INTERNAL_DELAYED_FILTER_H_

#include <type_traits>

#include "../../range.h"
#include "../../type_traits.h"
#include "../../utilities.h"

#include "../sequence_ops.h"

#include "common.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView, typename UnaryPredicate>
struct block_delayed_filter_t : public block_iterable_view_base<UnderlyingView, block_delayed_filter_t<UnderlyingView, UnaryPredicate>> {
 private:
  using base = block_iterable_view_base<UnderlyingView, block_delayed_filter_t<UnderlyingView, UnaryPredicate>>;
  using base::get_view;

 public:
  using value_type = range_value_type_t<UnderlyingView>;
  using reference = range_reference_type_t<UnderlyingView>;

  template<typename UV>
  block_delayed_filter_t(UV&& v, UnaryPredicate p_) : base(std::forward<UV>(v)), p(std::move(p_)) {}



  // Returns the number of blocks
  auto get_num_blocks() const { return num_blocks(get_view()); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) const { return block_iterator(begin_block(get_view(), i), this); }

  // Return the sentinel denoting the end of block i
  auto get_end_block(size_t i) const { return block_iterator(end_block(get_view(), i), this); }

  [[nodiscard]] size_t size() const { return get_view().size(); }

 private:
  copyable_function_wrapper<UnaryPredicate> p;
};

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FILTER_H_