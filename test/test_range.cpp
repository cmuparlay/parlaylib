#include "gtest/gtest.h"

#include <numeric>
#include <vector>

#include <parlay/delayed_sequence.h>
#include <parlay/parallel.h>
#include <parlay/range.h>
#include <parlay/slice.h>

template<PARLAY_RANGE_TYPE R>
auto f(R&& r) {
  return parlay::size(r);
}

TEST(TestRange, TestTemplate) {
  std::vector<int> a = {1,2,3};
  ASSERT_EQ(f(a), 3);
  
  const auto& a2 = a;
  ASSERT_EQ(f(a2), 3);
  
  ASSERT_EQ(f(std::vector<int>{1,2,3}), 3);
}

TEST(TestRange, TestSize) {
  std::vector<int> a = {1,2,3};
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestVector) {
  PARLAY_RANGE a = std::vector<int>{1,2,3};
  ASSERT_EQ(a.size(), 3);
}

TEST(TestRange, TestArray) {
  PARLAY_RANGE a = std::array<int, 3>{1,2,3};
  ASSERT_EQ(a.size(), 3);
}

TEST(TestRange, TestDelayedSequence) {
  PARLAY_RANGE a = parlay::delayed_seq<int>(10, [](int x) { return x; });
  ASSERT_EQ(a.size(), 10);
}

TEST(TestRange, TestSlice) {
  std::vector<int> a = {1,2,3};
  PARLAY_RANGE s = parlay::make_slice(a);
  ASSERT_EQ(s.size(), 3);
}
