#include "gtest/gtest.h"

#include <parlay/delayed_sequence.h>

TEST(TestDelayedSequence, TestConstruction) {
  auto s = parlay::delayed_seq<int>(1000, [](int i) { return i; });
  ASSERT_EQ(s.size(), 1000);
}


