// Useful reusable stuff for testing sorting functions

#ifndef PARLAY_TEST_SORTING_UTILS
#define PARLAY_TEST_SORTING_UTILS

// A pair that ignores its seconds element for comparisons
// Useful to test stable sorting algorithms.
struct UnstablePair {
  int x, y;
  bool operator<(const UnstablePair& other) const {
    return x < other.x;
  }
  bool operator>(const UnstablePair& other) const {
    return x > other.x;
  }
  bool operator==(const UnstablePair& other) const {
    return x == other.x && y == other.y;
  }
};

std::ostream& operator<<(std::ostream& o, UnstablePair x) {
  return o << x.x << ',' << x.y;
}

// A simple uncopyable object. Useful to test inplace sorting
// algorithms to ensure that no accidental copies are being
// made anywhere in the code.
struct UncopyableThing {
  int x;
  UncopyableThing(int _x) : x(_x) { }
  
  UncopyableThing(UncopyableThing&&) = default;
  UncopyableThing& operator=(UncopyableThing&&) = default;
  
  UncopyableThing(const UncopyableThing&) = delete;
  UncopyableThing& operator=(const UncopyableThing&) = delete;
  
  bool operator<(const UncopyableThing& other) const {
    return x < other.x;
  }
  bool operator==(const UncopyableThing& other) const {
    return x == other.x;
  }
};

// A self-referential object that always holds a pointer
// to itself. This is useful to check that copy and move
// constructors are being called rather than being
// circumvented by memcpying objects around.
struct SelfReferentialThing {
  int x;

  SelfReferentialThing() : x(0), me(this) { }
  SelfReferentialThing(int _x) : x(_x), me(this) { }
  
  SelfReferentialThing(const SelfReferentialThing& other) : x(other.x), me(this) {}
  SelfReferentialThing& operator=(const SelfReferentialThing& other) {
    x = other.x;
    me = this;
    return *this;
  }

  bool operator<(const SelfReferentialThing& other) const {
    return x < other.x;
  }
  bool operator==(const SelfReferentialThing& other) const {
    return x == other.x;
  }
  
  ~SelfReferentialThing() {
    assert(me == this);
  }
  
 private:
  SelfReferentialThing* me;
};

namespace std {
  template<>
  struct hash<SelfReferentialThing> {
    size_t operator()(const SelfReferentialThing& thing) const {
      return std::hash<int>{}(thing.x);
    }
  };
}

#endif  // PARLAY_TEST_SORTING_UTILS
