#include "gtest/gtest.h"

#include <algorithm>
#include <functional>
#include <numeric>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

TEST(TestDelayedReduce, TestRadReduceEmpty) {
  const parlay::sequence<int> a;
  ASSERT_TRUE(a.empty());
  auto x = parlay::delayed::reduce(a);
  ASSERT_EQ(x, 0);
}

TEST(TestDelayedReduce, TestRadReduce) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(10000));
  auto x = parlay::delayed::reduce(a);
  ASSERT_EQ(x, 49995000);
}

TEST(TestDelayedReduce, TestBidReduceEmpty) {
  const parlay::sequence<int> a;
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid);
  ASSERT_EQ(x, 0);
}

TEST(TestDelayedReduce, TestBidReduceSmall) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(1000));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid);
  static_assert(std::is_same_v<decltype(x), int>);
  ASSERT_EQ(x, 499500);
}

TEST(TestDelayedReduce, TestBidReduceSimple) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid);
  static_assert(std::is_same_v<decltype(x), int>);
  ASSERT_EQ(x, 1800030000);
}

TEST(TestDelayedReduce, TestBidReduceConstRef) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid);
  static_assert(std::is_same_v<decltype(x), int>);
  ASSERT_EQ(x, 1800030000);
}

TEST(TestDelayedReduce, TestBidReduceCustomOp) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid, parlay::bit_xor<int>{});
  auto actual_total = std::accumulate(std::begin(a), std::end(a), 0, std::bit_xor<>{});
  static_assert(std::is_same_v<decltype(x), int>);
  ASSERT_EQ(x, actual_total);
}

TEST(TestDelayedReduce, TestBidReduceCustomIdentity) {
  const parlay::sequence<unsigned int> a = parlay::to_sequence(parlay::iota<unsigned int>(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid, std::multiplies<>{}, 1U);
  auto actual_total = std::accumulate(std::begin(a), std::end(a), 1U, std::multiplies<>{});
  static_assert(std::is_same_v<decltype(x), unsigned int>);
  ASSERT_EQ(x, actual_total);
}

TEST(TestDelayedReduce, TestBidReduceCustomType) {
  const auto a = parlay::tabulate(50000, [&](size_t i) {
    parlay::BasicMatrix<int, 3> m;
    for (size_t j = 0; j < 3; j++) {
      for (size_t k = 0; k < 3; k++) {
        m(j, k) = i + j + k;
      }
    }
    return m;
  });

  auto add = [](auto&& x, auto&& y) { return parlay::matrix_add<3>(x,y); };

  const auto bid = parlay::block_iterable_wrapper(a);
  auto x = parlay::delayed::reduce(bid, add, parlay::BasicMatrix<int,3>::zero());
  auto actual_total = std::accumulate(std::begin(a), std::end(a), parlay::BasicMatrix<int,3>::zero(), add);
  static_assert(std::is_same_v<decltype(x), parlay::BasicMatrix<int,3>>);
  ASSERT_EQ(x, actual_total);
}

TEST(TestDelayedReduce, TestReduceNonConst) {
  parlay::NonConstRange r(60001);
  auto bid = parlay::block_iterable_wrapper(r);
  auto x = parlay::delayed::reduce(bid);
  static_assert(std::is_same_v<decltype(x), int>);
  ASSERT_EQ(x, 1800030000);
}
