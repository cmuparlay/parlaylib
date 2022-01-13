#include "gtest/gtest.h"

#include <numeric>
#include <type_traits>
#include <vector>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/range.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

TEST(TestDelayedForEach, TestRadForEachEmpty) {
  const parlay::sequence<int> seq;
  parlay::delayed::for_each(seq, [](int) { });
}

TEST(TestDelayedForEach, TestRadForEach) {
  parlay::sequence<int> seq(100000);
  parlay::delayed::for_each(parlay::iota(100000), [&](int i) { seq[i] = i; });

  for (size_t i = 0; i < seq.size(); i++) {
    ASSERT_EQ(seq[i], i);
  }
}

TEST(TestDelayedForEach, TestBidForEachEmpty) {
  const auto seq = parlay::block_iterable_wrapper(parlay::sequence<int>{});
  parlay::delayed::for_each(seq, [](int) { });
}

TEST(TestDelayedForEach, TestBidForEach) {
  parlay::sequence<int> seq(100000);
  parlay::delayed::for_each(parlay::block_iterable_wrapper(parlay::iota(100000)), [&](int i) { seq[i] = i; });

  for (size_t i = 0; i < seq.size(); i++) {
    ASSERT_EQ(seq[i], i);
  }
}
