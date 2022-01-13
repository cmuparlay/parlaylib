
#ifndef PARLAY_DELAYED_VIEWS_H_
#define PARLAY_DELAYED_VIEWS_H_

#include "internal/delayed/filter.h"
#include "internal/delayed/flatten.h"
#include "internal/delayed/map.h"
#include "internal/delayed/scan.h"
#include "internal/delayed/terminal.h"
#include "internal/delayed/zip.h"

namespace parlay {
namespace delayed {

using ::parlay::internal::delayed::to_sequence;

using ::parlay::internal::delayed::map;

using ::parlay::internal::delayed::flatten;

using ::parlay::internal::delayed::zip;

using ::parlay::internal::delayed::scan;

using ::parlay::internal::delayed::scan_inclusive;

using ::parlay::internal::delayed::reduce;

using ::parlay::internal::delayed::filter;

}
}

#endif  // PARLAY_DELAYED_VIEWS_H_
