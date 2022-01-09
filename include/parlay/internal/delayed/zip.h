
#ifndef PARLAY_INTERNAL_DELAYED_ZIP_H
#define PARLAY_INTERNAL_DELAYED_ZIP_H

#include <cstddef>

#include <algorithm>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common.h"

#include "../sequence_ops.h"

#include "../../range.h"

namespace parlay {
namespace internal {
namespace delayed {


template<typename... UnderlyingViews>
struct block_delayed_zip_t :
    public block_iterable_view_base<void, block_delayed_zip_t<UnderlyingViews...>> {

 private:
  using base = block_iterable_view_base<void, block_delayed_zip_t<UnderlyingViews...>>;

  static_assert(sizeof...(UnderlyingViews) >= 1);
  static_assert((is_block_iterable_range_v<UnderlyingViews> && ...));

 public:
  using value_type = std::tuple<range_value_type_t<UnderlyingViews>...>;
  using reference = std::tuple<range_reference_type_t<UnderlyingViews>...>;

  template<typename... UV>
  explicit block_delayed_zip_t(int, UV&&... vs) : base(), n_elements((std::min)({parlay::size(vs)...})),
                                                  n_blocks(num_blocks_from_size(n_elements)),
                                                  base_views(std::forward<UV>(vs)...) {
    //                         ^
    // Extra int parameter is used to ensure that this template doesn't
    // accidentally take over the job of the copy constructor.
    static_assert((std::is_same_v<UnderlyingViews, UV> && ...));
  }

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = std::conditional_t<Const,
        typename std::add_const_t<block_delayed_zip_t<UnderlyingViews...>>,
                                  block_delayed_zip_t<UnderlyingViews...>>;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::tuple<range_value_type_t<std::conditional_t<Const,
        typename std::add_const_t<UnderlyingViews>,
        UnderlyingViews>>...>;
    using reference = std::tuple<range_reference_type_t<std::conditional_t<Const,
        typename std::add_const_t<UnderlyingViews>,
        UnderlyingViews>>...>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    reference operator*() const {
      return std::apply([](auto&&... it) { return reference{std::forward<decltype(*it)>(*it)...}; }, its);
    }

    iterator_t& operator++() { index++; std::apply([](auto&... it) { (++it, ...); }, its); return *this; }
    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }

    // Comparisons only compare the first iterator in the sequence since
    // they should all be in sync, so a pair of iterators obtained from
    // the same zip are either all equal or all not equal.

    friend bool operator==(const iterator_t& x, const iterator_t& y) {
      return x.index == y.index;
    }

    friend bool operator!=(const iterator_t& x, const iterator_t& y) {
      return x.index != y.index;
    }

    // Conversion from non-const iterator to const iterator
    /* implicit */ iterator_t(const iterator_t<false>& other) : index(other.index), its(other.its) {}

    iterator_t() : its{} {}

   private:
    friend parent_type;

    iterator_t(size_t index_, range_iterator_type_t<UnderlyingViews>... its_) : index(index_), its(its_...) {}

    size_t index;
    std::tuple<range_iterator_type_t<UnderlyingViews>...> its;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  // Returns the number of blocks
  auto get_num_blocks() const { return n_blocks; }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) {
    return std::apply([i, n = n_elements](UnderlyingViews&... views) {
      return iterator(std::min(n, i * block_size), begin_block(views, i)...); }, base_views);
  }

  auto get_begin_block(size_t i) const {
    return std::apply([i, n = n_elements](const UnderlyingViews&... views) {
      return const_iterator(std::min(n, i * block_size), begin_block(views, i)...); }, base_views);
  }

  [[nodiscard]] size_t size() const { return n_elements; }

 private:
  size_t n_elements, n_blocks;
  std::tuple<view_storage_type<UnderlyingViews>...> base_views;
};

/*
// Given an NAry function f and a sequence of N ranges, produces the range
// consisting of f(*it1, *it2, ..., *itN)
template<typename NAryOperator, typename... UnderlyingViews>
struct block_delayed_zip_map_t :
    public block_iterable_view_base<void, block_delayed_zip_map_t<NAryOperator, UnderlyingViews...>> {

 private:
  using f_type = std::add_const<NAryOperator>;
  using base = block_delayed_zip_map_t<void, block_delayed_zip_map_t<NAryOperator, UnderlyingViews...>>;

  static_assert(sizeof...(UnderlyingViews) >= 1);
  static_assert((is_block_iterable_range_v<UnderlyingViews> && ...));
  static_assert(std::is_invocable_v<NAryOperator&, range_reference_type_t<UnderlyingViews>...>);

  // True if the supplied function is still invocable if the argument comes from
  // a const-qualified version of the input. (See map for a similar example)
  constexpr static inline bool const_invocable = std::is_invocable_v<std::add_const_t<NAryOperator>&>,
      range_reference_type_t<std::add_const_t<UnderlyingViews>>...>;

 public:
  using reference = std::invoke_result_t<NAryOperator&, range_reference_type_t<UnderlyingViews>...>;
  using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;

  template<typename... UV>
  explicit block_delayed_zip_map_t(NAryOperator f_, UV&&... vs) :
      base(), n_elements((std::min)({parlay::size(vs)...})), n_blocks(num_blocks_from_size(n_elements)),
      f(std::move(_f)), base_views(std::forward<UV>(vs)...) {
    static_assert((std::is_same_v<UnderlyingViews, UV> && ...));
  }

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = std::conditional_t<Const,
        typename std::add_const_t<block_delayed_iter_zip_map_t<NAryOperator, UnderlyingViews...>>,
                                  block_delayed_iter_zip_map_t<NAryOperator, UnderlyingViews...>>;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType<f_type, std::conditional_t<Const,
                                          typename std::add_const_t<UnderlyingViews>,
                                                                    UnderlyingViews>>...>;
    using reference = std::invoke_result_t<f_type, range_reference_type_t<std::conditional_t<Const,
                                                    typename std::add_const_t<UnderlyingViews>,
                                                                              UnderlyingViews>>...>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    reference operator*() const {
      return std::apply([](auto&&... it) { return reference{std::forward<decltype(it)>(it)...}; }, its);
    }

    iterator_t& operator++() { index++; std::apply([](auto&... it) { (++it, ...); }, its); return *this; }
    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }

    // Comparisons only compare the first iterator in the sequence since
    // they should all be in sync, so a pair of iterators obtained from
    // the same zip are either all equal or all not equal.

    friend bool operator==(const iterator_t& x, const iterator_t& y) {
      return x.index == y.index;
    }

    friend bool operator!=(const iterator_t& x, const iterator_t& y) {
      return x.index != y.index;
    }

    // Conversion from non-const iterator to const iterator
    /* implicit */ /*iterator_t(const iterator_t<false>& other) : index(other.index), its(other.its) {}

    iterator_t() : its{} {}

   private:
    friend parent_type;

    iterator_t(size_t index_, range_iterator_type_t<UnderlyingViews>... its_) : index(index_), its(its_...) {}

    size_t index;
    std::tuple<range_iterator_type_t<UnderlyingViews>...> its;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  // Returns the number of blocks
  auto get_num_blocks() const { return n_blocks; }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) {
    return std::apply([i, n = n_elements](UnderlyingViews&... views) {
      return iterator(std::min(n, i * block_size), begin_block(views, i)...); }, base_views);
  }

  auto get_begin_block(size_t i) const {
    return std::apply([i, n = n_elements](const UnderlyingViews&... views) {
      return const_iterator(std::min(n, i * block_size), begin_block(views, i)...); }, base_views);
  }

  [[nodiscard]] size_t size() const { return n_elements; }

 private:
  size_t n_elements, n_blocks;
  copyable_function_wrapper<NAryOperator> f;
  std::tuple<view_storage_type<UnderlyingViews>...> base_views;
};
*/

// Note: Fold expressions are bugged in MSVC until version 19.27, so the following
// fails to compile on earlier versions :(

template<typename... UnderlyingViews,
    std::enable_if_t<(is_random_access_range_v<UnderlyingViews> && ...), int> = 0>
auto zip(UnderlyingViews&&... vs) {
  static_assert(sizeof...(UnderlyingViews) >= 1, "zip should have at least one argument");
  using value_type = std::tuple<range_value_type_t<UnderlyingViews>...>;
  using reference_type = std::tuple<range_reference_type_t<UnderlyingViews>...>;

  auto size = (std::min)({parlay::size(vs)...});
  return parlay::internal::delayed_tabulate<reference_type, value_type>(size,
    [views = std::tuple<UnderlyingViews...>(std::forward<UnderlyingViews>(vs)...)](size_t i) {
      return std::apply([i](auto&&... views) { return reference_type(std::begin(views)[i]...); }, views);
    });
}

template<typename... UnderlyingViews,
    std::enable_if_t<((!is_random_access_range_v<UnderlyingViews> || ...) &&
                       (is_block_iterable_range_v<UnderlyingViews> && ...)), int> = 0>
auto zip(UnderlyingViews&&... vs) {
  static_assert(sizeof...(UnderlyingViews) >= 1, "zip should have at least one argument");
  return block_delayed_zip_t<UnderlyingViews...>(0, std::forward<UnderlyingViews>(vs)...);
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_ZIP_H
