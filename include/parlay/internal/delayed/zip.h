
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

// Note: Fold expressions are bugged in MSVC until version 19.27, so the following
// fails to compile on earlier versions :(

template<typename... UnderlyingViews,
    std::enable_if_t<(is_random_access_range_v<UnderlyingViews> && ...), int> = 0>
auto zip(UnderlyingViews&&... vs) {
  using value_type = std::tuple<range_value_type_t<UnderlyingViews>...>;
  using reference_type = std::tuple<range_reference_type_t<UnderlyingViews>...>;

  auto size = (std::min)({parlay::size(vs)...});
  return parlay::internal::delayed_tabulate<reference_type, value_type>(size,
    [views = std::tuple<UnderlyingViews...>(std::forward<UnderlyingViews>(vs)...)](size_t i) {
      return std::apply([i](auto&&... views) { return reference_type(std::begin(views)[i]...); }, views);
    });
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_ZIP_H
