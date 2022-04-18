
#ifndef PARLAY_DELAYED_H_
#define PARLAY_DELAYED_H_

#include <cstddef>

#include <tuple>
#include <type_traits>
#include <utility>

#include "internal/delayed/filter.h"        // IWYU pragma: export
#include "internal/delayed/filter_op.h"     // IWYU pragma: export
#include "internal/delayed/flatten.h"       // IWYU pragma: export
#include "internal/delayed/map.h"           // IWYU pragma: export
#include "internal/delayed/scan.h"          // IWYU pragma: export
#include "internal/delayed/terminal.h"      // IWYU pragma: export
#include "internal/delayed/zip.h"           // IWYU pragma: export

#include "internal/sequence_ops.h"

#include "delayed_sequence.h"
#include "range.h"
#include "type_traits.h"

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

template<size_t N, typename Range_>
auto elements_view(Range_&& r) {
  static_assert(is_block_iterable_range_v<Range_>);
  static_assert(N < std::tuple_size_v<range_value_type_t<Range_>>);
  using return_type = maybe_decay_t<!std::is_reference_v<range_reference_type_t<Range_>>,
        decltype(std::get<N>(std::declval<range_reference_type_t<Range_>>()))>;
  return ::parlay::internal::delayed::map(std::forward<Range_>(r),
    [](auto&& x) -> return_type { return std::get<N>(std::forward<decltype(x)>(x)); });
}

// Given a range of pair-like objects (e.g. pairs or tuples of size two),
// returns a delayed view of the first elements of the pairs
template<typename Range_>
auto keys_view(Range_&& r) {
  static_assert(is_block_iterable_range_v<Range_>);
  return elements_view<0>(std::forward<Range_>(r));
}

// Given a range of pair-like objects (e.g. pairs or tuples of size two),
// returns a delayed view of the second elements of the pairs
template<typename Range_>
auto values_view(Range_&& r) {
  static_assert(is_block_iterable_range_v<Range_>);
  return elements_view<1>(std::forward<Range_>(r));
}

}
}

#endif  // PARLAY_DELAYED_H_
