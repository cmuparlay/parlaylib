
#ifndef PARLAY_INTERNAL_UNINITIALIZED_STORAGE_H_
#define PARLAY_INTERNAL_UNINITIALIZED_STORAGE_H_

#include <memory>
#include <new>

#include "debug_uninitialized.h"

namespace parlay {
namespace internal {

// Contains uninitialized correctly aligned storage large enough to
// hold a single object of type T. The object is not initialized
// and its destructor is not ran when the storage goes out of
// scope. The purpose of such an object is to act as temp space
// when using uninitialized_relocate
template<typename T>
class uninitialized_storage {
  using value_type = T;
  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

 public:
  uninitialized_storage() {
#ifdef PARLAY_DEBUG_UNINITIALIZED
    // If uninitialized memory debugging is turned on, make sure that
    // each object of type UninitializedTracker is appropriately set
    // to its uninitialized state.
    if constexpr (std::is_same_v<value_type, UninitializedTracker>) {
      get()->initialized = false;
    }
#endif
  }

#ifdef PARLAY_DEBUG_UNINITIALIZED
  ~uninitialized_storage() {
    PARLAY_ASSERT_UNINITIALIZED(*get());
  }
#endif

  value_type* get() {
    return std::launder(reinterpret_cast<value_type*>(std::addressof(storage)));
  }

  const value_type* get() const {
    return std::launder(reinterpret_cast<value_type*>(std::addressof(storage)));
  }
};

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_UNINITIALIZED_STORAGE_H_