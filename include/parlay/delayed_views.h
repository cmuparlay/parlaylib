
#ifndef PARLAY_DELAYED_VIEWS_H_
#define PARLAY_DELAYED_VIEWS_H_

#include <algorithm>

#include "range.h"

#include "internal/sequence_ops.h"

#include "internal/delayed/common.h"
#include "internal/delayed/map.h"

namespace parlay {
namespace delayed {

template<typename UnderlyingView, typename UnaryOperator,
    std::enable_if_t<is_random_access_range_v<UnderlyingView>, int> = 0>
auto map(UnderlyingView&& v, UnaryOperator f) {
  return parlay::internal::delayed_map(std::forward<UnderlyingView>(v), std::move(f));
}

template<typename UnderlyingView, typename UnaryOperator,
    std::enable_if_t<!is_random_access_range_v<UnderlyingView> &&
        parlay::is_block_iterable_range_v<UnderlyingView>, int> = 0>
auto map(UnderlyingView&& v, UnaryOperator f) {
  return parlay::internal::delayed::block_delayed_map_t<UnderlyingView, UnaryOperator>
      (std::forward<UnderlyingView>(v), std::move(f));
}

template<typename... UnderlyingViews,
    std::enable_if_t<(is_random_access_range_v<UnderlyingViews> && ...), int> = 0>
auto zip(UnderlyingViews&&... vs) {
  using value_type = std::tuple<range_value_type_t<UnderlyingViews>...>;
  using reference_type = std::tuple<range_reference_type_t<UnderlyingViews>...>;

  auto size = std::min({parlay::size(vs)...});
  return parlay::internal::delayed_tabulate<reference_type, value_type>(size,
    [views = std::tuple<UnderlyingViews...>(std::forward<UnderlyingViews>(vs)...)](size_t i) {
      return std::apply([i](auto&&... views) { return reference_type(std::begin(views)[i]...); }, views);
    });
}


}
}

#endif  // PARLAY_DELAYED_VIEWS_H_
