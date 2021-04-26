// The mid-level sequence implementation, which builds on the storage
// base class, sequence_base, and extends it with parallel operations
// for working with sequences. It supports all of the operations
// supported by std::vector.
//
// Small-size optimization is enabled by a template parameter, which
// allows sequences of trivial types whose elements fit into 15 bytes
// to be stored inline with no heap allocations
//

#ifndef PARLAY_INTERNAL_SEQUENCE_IMPL_H
#define PARLAY_INTERNAL_SEQUENCE_IMPL_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "../alloc.h"
#include "../parallel.h"
#include "../range.h"
#include "../slice.h"
#include "../type_traits.h"      // IWYU pragma: keep  // for is_trivially_relocatable
#include "../utilities.h"

#include "sequence_base.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "debug_uninitialized.h"
#endif

namespace parlay {
namespace internal {

// Parlay's mid-level sequence implementation
//
// This class implements high-level operations on sequences, such
// as insertion and deletion. Low-level details regarding storage
// and allocation and handled by the base class, sequence_base


}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_SEQUENCE_IMPL_H
