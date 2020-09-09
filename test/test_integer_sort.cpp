#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#define PARLAY_DEBUG_UNINITIALIZED
#include <parlay/internal/debug_uninitialized.h>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "sorting_utils.h"

TEST(TestIntegerSort, TestUninitialized) {
  auto s = parlay::sequence<parlay::debug_uninitialized>::uninitialized(10000000);
  parlay::parallel_for(0, 10000000, [&](size_t i) {
    s[i].x = (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::integer_sort(s, [](auto s) { return s.x; });
  ASSERT_EQ(s.size(), sorted.size());
}
