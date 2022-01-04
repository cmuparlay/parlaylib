
#ifndef PARLAY_DELAYED_VIEWS_H_
#define PARLAY_DELAYED_VIEWS_H_

#include <algorithm>
#include <memory>

#include "parallel.h"
#include "range.h"
#include "sequence.h"

#include "internal/sequence_ops.h"

#include "internal/delayed/common.h"
#include "internal/delayed/flatten.h"
#include "internal/delayed/map.h"

namespace parlay {
namespace delayed {

using ::parlay::internal::delayed::to_sequence;

using ::parlay::internal::delayed::map;

using ::parlay::internal::delayed::flatten;



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
