#ifndef PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_
#define PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_

namespace parlay {

// A simple type to help look for uninitialized memory bugs.
// By default, debug_uninitialized contains x = -1, which is
// a placeholder that pretends to be uninitialized memory.
//
// Attempting to copy or assign to an "uninitialized" object
// will result in an assertion failure.
//
// The functions assign_uninitialized and move_uninitialized
// are also augmented to check whether their destinations are
// "uninitialized" when using this type.
//
// Any object with x != -1 is considered to be initialized
// memory.
struct debug_uninitialized {

  debug_uninitialized() : x(-1) { }

  debug_uninitialized(const debug_uninitialized& other) {
    // Ensure that the other object is initialized
    assert(other.x != -1 && "Copying an uninitialized variable");

    // Copy
    x = other.x;
  }

  debug_uninitialized& operator=(const debug_uninitialized& other) {
    // Ensure that this object is initialized
    assert(x != -1 && "Assigning to an uninitialized variable");

    // Ensure that the other object is initialized
    assert(other.x != -1 && "Copy assigning an uninitialized variable");

    // Copy
    x = other.x;

    return *this;
  }

  int x;
};
  
}

#endif  // PARLAY_INTERNAL_DEBUG_UNINITIALIZED_H_
