#include "gtest/gtest.h"

#include <string>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

TEST(TestDelayedMap, TestRadMapSimple) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));

  auto m = parlay::delayed::map(a, [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestRadMapReference) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));

  auto m = parlay::delayed::map(a, [](int& x) -> int& { return x; });

  size_t i = 0;
  for (int& x : m) {
    ASSERT_EQ(i, x);
    ASSERT_EQ(a[i], x);
    x++;
    ASSERT_EQ(i+1, x);
    ASSERT_EQ(a[i], x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleRef) {
  auto bid = parlay::internal::delayed::block_iterable_wrapper(parlay::iota(100001));

  auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleMove) {
  auto bid = parlay::internal::delayed::block_iterable_wrapper(parlay::iota(100001));

  auto m = parlay::delayed::map(std::move(bid), [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleRefRef) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));
  auto bid = parlay::internal::delayed::block_iterable_wrapper(a);

  auto m = parlay::delayed::map(bid, [](int& x) -> int& { return x; });

  size_t i = 0;
  for (int& x : m) {
    ASSERT_EQ(i, x);
    ASSERT_EQ(a[i], x);
    x++;
    ASSERT_EQ(i+1, x);
    ASSERT_EQ(a[i], x);
    i++;
  }
}

TEST(TestDelayedMap, TestRadMapMoveRvalueRef) {
  parlay::sequence<std::vector<int>> s = {
      {0,1,2},
      {3,4,5},
      {6,7,8}
  };

  // Map the contents of s to rvalue references => they should be moved into the copy
  auto m = parlay::delayed::map(s, [](std::vector<int>& x) -> std::vector<int>&& { return std::move(x); });
  auto seq = parlay::delayed::to_sequence<std::vector<int>>(m);

  ASSERT_EQ(seq.size(), 3);
  ASSERT_EQ(seq[0].size(), 3);
  ASSERT_EQ(seq[1].size(), 3);
  ASSERT_EQ(seq[2].size(), 3);

  // If the input was moved from, it will now be empty
  ASSERT_TRUE(s[0].empty());
  ASSERT_TRUE(s[1].empty());
  ASSERT_TRUE(s[2].empty());
}

TEST(TestDelayedMap, TestBidMapMoveRvalueRef) {
  parlay::sequence<std::vector<int>> s = {
      {0,1,2},
      {3,4,5},
      {6,7,8}
  };
  auto bid = parlay::internal::delayed::block_iterable_wrapper(s);

  // Map the contents of s to rvalue references => they should be moved into the copy
  auto m = parlay::delayed::map(bid, [](std::vector<int>& x) -> std::vector<int>&& { return std::move(x); });
  auto seq = parlay::delayed::to_sequence<std::vector<int>>(m);

  ASSERT_EQ(seq.size(), 3);
  ASSERT_EQ(seq[0].size(), 3);
  ASSERT_EQ(seq[1].size(), 3);
  ASSERT_EQ(seq[2].size(), 3);

  // If the input was moved from, it will now be empty
  ASSERT_TRUE(s[0].empty());
  ASSERT_TRUE(s[1].empty());
  ASSERT_TRUE(s[2].empty());
}
