#include "gtest/gtest.h"

#include <parlay/delayed_sequence.h>

TEST(TestDelayedSequence, TestConstruction) {
  auto s = parlay::delayed_seq<int>(1000, [](size_t i) -> int { return i; });
  ASSERT_EQ(s.size(), 1000);
}


