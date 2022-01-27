#ifndef PARLAY_INTERNAL_DELAYED_MAP_H_
#define PARLAY_INTERNAL_DELAYED_MAP_H_

#include <cstddef>

#include <iterator>
#include <type_traits>
#include <utility>

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
  using base = block_iterable_view_base<UnderlyingView, block_delayed_map_t>;
  using base::base_view;

  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(!std::is_reference_v<UnaryOperator>);
  static_assert(std::is_invocable_v<UnaryOperator&, range_reference_type_t<UnderlyingView>>);

 public:

  using reference = std::invoke_result_t<UnaryOperator&, range_reference_type_t<UnderlyingView>>;
  using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;

  template<typename UV>
  block_delayed_map_t(UV&& v, UnaryOperator f) : base(std::forward<UV>(v), 0), op(std::move(f)) {}

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = maybe_const_t<Const, block_delayed_map_t>;
    using base_view_type = maybe_const_t<Const, std::remove_reference_t<UnderlyingView>>;
    using base_iterator_type = range_iterator_type_t<base_view_type>;
    using f_type = maybe_const_t<Const, UnaryOperator>;

   public:
    static_assert(std::is_invocable_v<f_type&, range_reference_type_t<base_view_type>>);
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

    // Conversion from non-const iterator to const iterator
    /* implicit */ iterator_t(const iterator_t<false>& other) : it(other.it), op(other.op) {}

    iterator_t() : it{}, op(nullptr) {}

   private:
    friend parent_type;

    iterator_t(base_iterator_type it_, f_type* op_) : it(std::move(it_)), op(op_) {}

    base_iterator_type it;
    f_type* op;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  // Returns the number of elements in the range
  [[nodiscard]] size_t size() const { return base_view().size(); }

  // Returns the number of blocks in the range
  auto get_num_blocks() const { return num_blocks(base_view()); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return iterator(begin_block(base_view(), i), op.get()); }

  // Return an iterator pointing to the beginning of block i
  template<typename UV = UnderlyingView, std::enable_if_t<is_range_const_transformable_v<UV, UnaryOperator>, int> = 0>
  auto get_begin_block(size_t i) const { return const_iterator(begin_block(base_view(), i), op.get()); }

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