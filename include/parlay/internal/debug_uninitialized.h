#ifndef PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_
#define PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_

#include <type_traits>

namespace parlay {
namespace internal {

// A simple type to help look for uninitialized memory bugs.
// UninitializedTracker is essentially an integer type, but
// also tracks whether it is currently in an initialized
// or uninitialized state.
//
// Attempting to assign to an uninitialized state will
// trigger an assertion failure. Attempting to copy or
// move an uninitialized object will also cause failure.
//
// This code will technically invoke undefined behaviour
// at some point, since we set the initialized flag to
// false in the destructor for the sole purpose of
// checking whether it is indeed false the next time
// someone comes along and wants to do an uninitialized
// in place construction at that memory location.
//
// Some compilers or debugging tools might therefore not
// work correctly with this class. Defining the initialized
// flag as volatile seems to prevent compilers from optimizing
// it out, and therefore leaves its value in memory after
// the object is destroyed, but this can not be guaranteed.
//
// For correctness, this type should only ever be used
// if its memory is allocated inside a parlay::sequence
// or parlay::uninitialized_sequence since they know how
// to set the initialized/uninitialized flag
struct UninitializedTracker {

  UninitializedTracker() : x(0), initialized(true) {}

  // cppcheck-suppress noExplicitConstructor
  /* implicit */ UninitializedTracker(int _x) : x(_x), initialized(true) {}

  UninitializedTracker(const UninitializedTracker &other) {
    assert(other.initialized && "Attempting to copy an uninitialized object!");
    x = other.x;
    initialized = true;
  }

  UninitializedTracker &operator=(const UninitializedTracker &other) {
    assert(initialized && "Attempting to assign to an uninitialized object!");
    assert(other.initialized && "Copy assigning an uninitialized object!");
    x = other.x;
    return *this;
  }

  ~UninitializedTracker() {
    assert(initialized && "Destructor called on uninitialized object!");
    initialized = false;
  }

  bool operator==(const UninitializedTracker& other) const {
    assert(initialized && "Trying to compare an uninitialized object!");
    assert(other.initialized && "Trying to compare against an uninitialized object!");
    return x == other.x;
  }

  bool operator<(const UninitializedTracker& other) const {
    assert(initialized && "Trying to compare an uninitialized object!");
    assert(other.initialized && "Trying to compare against an uninitialized object!");
    return x < other.x;
  }

  void swap(UninitializedTracker& other) {
    assert(initialized && "Trying to swap uninitialized object!");
    assert(other.initialized && "Trying to swap with an uninitialized object!");
    std::swap(x, other.x);
  }

  friend void swap(parlay::internal::UninitializedTracker& a, parlay::internal::UninitializedTracker& b) {
    a.swap(b);
  }

  int x;
  volatile bool initialized;   // Volatile required to prevent optimizing out the store in the destructor
};

}  // namespace internal
}  // namespace parlay


// Macros for testing initialized/uninitializedness

#ifdef PARLAY_DEBUG_UNINITIALIZED

// Checks that the given UninitializedTracker object is uninitialized
//
// Usage:
//   PARLAY_ASSERT_UNINITIALIZED(x)
// Result:
//   Terminates the program if the expression (x) refers to an object of
//   type UninitializedTracker such that (x).initialized is true, i.e.
//   the object is in an initialized state
//
// Note: This macro should only ever be used on memory that was allocated
// by a parlay::sequence or a parlay::uninitialized_sequence, since they
// are required to appropriately flag all newly allocated memory as being
// uninitialized. False positives will occur if this macro is invoked on
// memory that is not managed by one of these containers.
#define PARLAY_ASSERT_UNINITIALIZED(x)                                                       \
do {                                                                                         \
  using PARLAY_AU_T = std::remove_cv_t<std::remove_reference_t<decltype(x)>>;                \
  if constexpr (std::is_same_v<PARLAY_AU_T, ::parlay::internal::UninitializedTracker>) {     \
    assert(!((x).initialized) && "Memory required to be uninitialized is initialized!");     \
  }                                                                                          \
} while (0)

// Checks that the given UninitializedTracker object is initialized
//
// Usage:
//   PARLAY_ASSERT_INITIALIZED(x)
// Result:
//   Terminates the program if the expression (x) refers to an object of
//   type UninitializedTracker such that (x).initialized is false, i.e.
//   the object is in an uninitialized state
//
// Note: This macro should only ever be used on memory that was allocated
// by a parlay::sequence or a parlay::uninitialized_sequence, since they
// are required to appropriately flag all newly allocated memory as being
// uninitialized. False positives will occur if this macro is invoked on
// memory that is not managed by one of these containers.
#define PARLAY_ASSERT_INITIALIZED(x)                                                         \
do {                                                                                         \
  using PARLAY_AI_T = std::remove_cv_t<std::remove_reference_t<decltype(x)>>;                \
  if constexpr (std::is_same_v<PARLAY_AI_T, ::parlay::internal::UninitializedTracker>) {     \
    assert((x).initialized && "Memory required to be initialized is uninitialized!");        \
  }                                                                                          \
} while (0)

#else
#define PARLAY_ASSERT_UNINITIALIZED(x)
#define PARLAY_ASSERT_INITIALIZED(x)
#endif  // PARLAY_DEBUG_UNINITIALIZED

#endif  // PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_
