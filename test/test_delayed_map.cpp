#include "gtest/gtest.h"

#include <string>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

TEST(TestDelayedMap, TestRadMapSimple) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100000));

  auto m = parlay::delayed::map(a, [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleRef) {
  auto bid = parlay::internal::delayed::block_iterable_wrapper(parlay::iota(100000));

  auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleMove) {
  auto bid = parlay::internal::delayed::block_iterable_wrapper(parlay::iota(100000));

  auto m = parlay::delayed::map(std::move(bid), [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}
