#include "gtest/gtest.h"

#include <numeric>
#include <vector>

#include <parlay/delayed_sequence.h>
#include <parlay/parallel.h>
#include <parlay/slice.h>

TEST(TestSlice, TestConstruction) {
  std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a);
  ASSERT_EQ(std::begin(a), std::begin(s));
  ASSERT_EQ(std::end(a), std::end(s));
}

TEST(TestSlice, TestConstructionConst) {
  const std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a);
  ASSERT_EQ(std::cbegin(a), std::begin(s));
  ASSERT_EQ(std::cend(a), std::end(s));
}

TEST(TestSlice, TestDirectConstruction) {
  std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a.begin(), a.end());
  ASSERT_EQ(std::begin(a), std::begin(s));
  ASSERT_EQ(std::end(a), std::end(s));
}

TEST(TestSlice, TestCopyConstruct) {
  std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a);
  auto s2 = s;
  ASSERT_EQ(std::begin(a), std::begin(s2));
  ASSERT_EQ(std::end(a), std::end(s2));
}

TEST(TestSlice, TestCopyAssign) {
  std::vector<int> a = {1,2,3};
  std::vector<int> a2 = {4,5,6};
  auto s = parlay::make_slice(a);
  auto s2 = parlay::make_slice(a2);
  s2 = s;
  ASSERT_EQ(std::begin(a), std::begin(s2));
  ASSERT_EQ(std::end(a), std::end(s2));
}

TEST(TestSlice, TestSubscript) {
  std::vector<int> a(1000);
  std::iota(std::begin(a), std::end(a), 0);
  auto s = parlay::make_slice(a);
  for (size_t i = 0; i < 1000; i++) {
    ASSERT_EQ(s[i], a[i]);
  }
}

TEST(TestSlice, TestSize) {
  std::vector<int> a(1000);
  auto s = parlay::make_slice(a);
  ASSERT_EQ(s.size(), 1000);
}

TEST(TestSlice, TestCut) {
  std::vector<int> a(1000);
  std::iota(std::begin(a), std::end(a), 0);
  auto s = parlay::make_slice(a);
  auto s2 = s.cut(200,400);
  for (size_t i = 0; i < 200; i++) {
    ASSERT_EQ(s2[i], a[i+200]);
  }
}

TEST(TestSlice, TestReadOnlyView) {
  std::vector<int> a(1000);
  std::iota(std::begin(a), std::end(a), 0);
  const auto& ar = a;
  auto s = parlay::make_slice(ar);
  for (size_t i = 0; i < 1000; i++) {
    ASSERT_EQ(s[i], a[i]);
  }
}

TEST(TestSlice, TestMutableView) {
  std::vector<int> a(1000);
  std::iota(std::begin(a), std::end(a), 0);
  auto s = parlay::make_slice(a);
  for (size_t i = 0; i < 1000; i++) {
    ASSERT_EQ(s[i], i);
    ASSERT_EQ(a[i], i);
    s[i] += 1;
    ASSERT_EQ(s[i], i+1);
    ASSERT_EQ(a[i], i+1);
  }
}

TEST(TestSlice, TestDelayedSequence) {
  auto ds = parlay::delayed_seq<int>(1000, [](int x) { return x; });
  auto s = parlay::make_slice(ds);
  for (size_t i = 0; i < 1000; i++) {
    ASSERT_EQ(s[i], ds[i]);
  }
}
