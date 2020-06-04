#include "gtest/gtest.h"

#include <numeric>
#include <vector>

#include <parlay/parallel.h>
#include <parlay/range.h>

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