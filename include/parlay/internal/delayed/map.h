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
struct block_delayed_map_t :
    public block_iterable_view_base<UnderlyingView, block_delayed_map_t<UnderlyingView, UnaryOperator>> {

 private:
  static_assert(is_block_iterable_range_v<UnderlyingView>);
  using base = block_iterable_view_base<UnderlyingView, block_delayed_map_t<UnderlyingView, UnaryOperator>>;
  using base::base_view;

  using non_const_base_view_type = std::remove_reference_t<UnderlyingView>;
  using const_base_view_type = std::add_const_t<std::remove_reference_t<UnderlyingView>>;

 public:
  static_assert(std::is_invocable_v<std::add_const_t<UnaryOperator>, range_reference_type_t<non_const_base_view_type>>);
  using reference = std::invoke_result_t<UnaryOperator, range_reference_type_t<non_const_base_view_type>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;

  // True if the supplied function is still invocable if the argument comes from
  // a const-qualified version of the input. For example, a function that takes
  // its argument as (const int& x) is invocable from a const input, but a function
  // whose argument is (int& x) would not be.
  constexpr static inline bool const_invocable = std::is_invocable_v<std::add_const_t<UnaryOperator>,
      range_reference_type_t<const_base_view_type>>;

  template<typename UV>
  block_delayed_map_t(UV&& v, UnaryOperator f) : base(std::forward<UV>(v)), op(std::move(f)) {}

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = block_delayed_map_t<UnderlyingView, UnaryOperator>;
    using base_view_type = std::conditional_t<Const, const_base_view_type, non_const_base_view_type>;
    using base_iterator_type = range_iterator_type_t<base_view_type>;

   public:
    static_assert(std::is_invocable_v<std::add_const_t<UnaryOperator>, range_reference_type_t<base_view_type>>);
    using iterator_category = std::forward_iterator_tag;
    using reference = std::invoke_result_t<UnaryOperator, range_reference_type_t<base_view_type>>;
    using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    decltype(auto) operator*() const { return (*op)(*it); }

    iterator_t& operator++() { ++it; return *this; }
    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }

    friend bool operator==(const iterator_t& x, const iterator_t& y) { return x.it == y.it; }
    friend bool operator!=(const iterator_t& x, const iterator_t& y) { return x.it != y.it; }

   private:
    friend parent_type;

    iterator_t() : it{}, op(nullptr) {}
    iterator_t(base_iterator_type it_, const UnaryOperator* op_) : it(it_), op(op_) {}

    base_iterator_type it;
    const UnaryOperator* op;
  };

  // If the function is not const invocable, just use a non-const iterator as the
  // const iterator and hope that the user never calls a const-qualified function.
  // If they do, it will fail anyway.
  using iterator = iterator_t<false>;
  using const_iterator = std::conditional_t<const_invocable, iterator_t<true>, iterator_t<false>>;

  // Returns the number of blocks
  auto get_num_blocks() const { return num_blocks(base_view()); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return iterator(begin_block(base_view(), i), op.get()); }
  auto get_begin_block(size_t i) const { return const_iterator(begin_block(base_view(), i), op.get()); }

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