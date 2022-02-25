
#ifndef PARLAY_DELAYED_H_
#define PARLAY_DELAYED_H_

#include <cstddef>

#include <tuple>
#include <utility>

#include "internal/delayed/filter.h"
#include "internal/delayed/filter_op.h"
#include "internal/delayed/flatten.h"
#include "internal/delayed/map.h"
#include "internal/delayed/scan.h"
#include "internal/delayed/terminal.h"
#include "internal/delayed/zip.h"

#include "internal/sequence_ops.h"

#include "delayed_sequence.h"
#include "range.h"

namespace parlay {
namespace delayed {

// Import all first-class delayed operations

using ::parlay::internal::delayed::to_sequence;
using ::parlay::internal::delayed::reduce;
using ::parlay::internal::delayed::map;
using ::parlay::internal::delayed::flatten;
using ::parlay::internal::delayed::zip;
using ::parlay::internal::delayed::scan;
using ::parlay::internal::delayed::scan_inclusive;
using ::parlay::internal::delayed::filter;
using ::parlay::internal::delayed::filter_op;
using ::parlay::internal::delayed::map_maybe;
using ::parlay::internal::delayed::for_each;
using ::parlay::internal::delayed::apply;

// Delayed tabulate

template<typename UnaryFunction>
auto tabulate(size_t n, UnaryFunction f) {
  return parlay::internal::delayed_tabulate(n, std::move(f));
}

// Composite delayed operations

template<typename Integral_>
auto iota(Integral_ n) {
  static_assert(std::is_integral_v<Integral_>);
  return delayed_seq<Integral_>(n, [](size_t i) -> Integral_ { return i; });
}

template<typename Range_>
auto enumerate(Range_&& r) {
  static_assert(is_block_iterable_range_v<Range_>);
  return ::parlay::internal::delayed::zip(::parlay::delayed::iota(parlay::size(r)), std::forward<Range_>(r));
}

template<typename NaryOperator, typename... Ranges_>
auto zip_with(NaryOperator f, Ranges_&&... rs) {
  static_assert((is_block_iterable_range_v<Ranges_> && ...));
  static_assert(std::is_invocable_v<NaryOperator, range_reference_type_t<Ranges_>...>);
  return ::parlay::internal::delayed::map(
    ::parlay::internal::delayed::zip(std::forward<Ranges_>(rs)...),
    [f = std::move(f)](auto&& t) { return std::apply(f, std::forward<decltype(t)>(t)); }
  );
}

}
}

#endif  // PARLAY_DELAYED_H_
