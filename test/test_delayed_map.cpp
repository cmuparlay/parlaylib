#include "gtest/gtest.h"

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

// Check that mapped ranges are copyable and movable
struct Op { int operator()(int) const; };
using si = parlay::sequence<int>;
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::map(std::declval<si>(), Op{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::map(std::declval<si>(), Op{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::map(std::declval<si&>(), Op{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::map(std::declval<si&>(), Op{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::map(std::declval<si>(), Op{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::map(std::declval<si>(), Op{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::map(std::declval<si&>(), Op{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::map(std::declval<si&>(), Op{}))>);

using bdsi = decltype(parlay::block_iterable_wrapper(std::declval<si>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::map(std::declval<bdsi>(), Op{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::map(std::declval<bdsi>(), Op{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::map(std::declval<bdsi&>(), Op{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::map(std::declval<bdsi&>(), Op{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::map(std::declval<bdsi>(), Op{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::map(std::declval<bdsi>(), Op{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::map(std::declval<bdsi&>(), Op{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::map(std::declval<bdsi&>(), Op{}))>);

//static_assert(!parlay::is_forward_range_v<void>);

// ---------------------------------------------------------------------------------------
//                                     RAD VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedMap, TestRadMapSimple) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));

  auto m = parlay::delayed::map(a, [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestRadMapOwning) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));

  auto m = parlay::delayed::map(std::move(a), [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestRadMapConst) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));

  const auto m = parlay::delayed::map(a, [](int x) { return x + 1; });

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

TEST(TestDelayedMap, TestRadMapNonConstRange) {
  parlay::NonConstRange r(50000);

  auto m = parlay::delayed::map(r, [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

// ---------------------------------------------------------------------------------------
//                                     BID VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedMap, TestBidMapSeqRef) {
  const auto s = parlay::to_sequence(parlay::iota(1000001));
  const auto bid = parlay::block_iterable_wrapper(s);

  auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapNonOwning) {
  const auto bid = parlay::block_iterable_wrapper(parlay::iota(100001));

  auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapToSeq) {
  const auto bid = parlay::block_iterable_wrapper(parlay::iota(100001));

  const auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });
  ASSERT_EQ(m.size(), bid.size());

  auto s = parlay::delayed::to_sequence(m);
  ASSERT_EQ(s.size(), m.size());

  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], i+1);
  }
}

TEST(TestDelayedMap, TestBidMapSimpleMove) {
  auto bid = parlay::block_iterable_wrapper(parlay::iota(100001));

  auto m = parlay::delayed::map(std::move(bid), [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapConstRef) {
  auto bid = parlay::block_iterable_wrapper(parlay::iota(100001));

  const auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapConstOwner) {
  auto bid = parlay::block_iterable_wrapper(parlay::iota(100001));

  const auto m = parlay::delayed::map(std::move(bid), [](int x) { return x + 1; });

  size_t i = 0;
  for (auto x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}

TEST(TestDelayedMap, TestBidMapSimpleRefRef) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));
  auto bid = parlay::block_iterable_wrapper(a);

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


TEST(TestDelayedMap, TestBidMapMoveRvalueRef) {
  parlay::sequence<std::vector<int>> s = {
      {0,1,2},
      {3,4,5},
      {6,7,8}
  };
  auto bid = parlay::block_iterable_wrapper(s);

  // Map the contents of s to rvalue references => they should be moved into the copy
  auto m = parlay::delayed::map(bid, [](std::vector<int>& x) -> std::vector<int>&& { return std::move(x); });
  static_assert(std::is_same_v<parlay::range_reference_type_t<decltype((m))>, std::vector<int>&&>);
  static_assert(parlay::is_block_iterable_range_v<decltype((m))>);
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

TEST(TestDelayedMap, TestBidMapNonConstRange) {
  parlay::NonConstRange r(50000);
  auto bid = parlay::block_iterable_wrapper(r);

  auto m = parlay::delayed::map(bid, [](int x) { return x + 1; });

  size_t i = 0;
  for (int x : m) {
    ASSERT_EQ(i+1, x);
    i++;
  }
}
