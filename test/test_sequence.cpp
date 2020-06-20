#include <list>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include <parlay/dynamic_sequence.h>


TEST(TestSequence, TestDefaultConstruct) {
  auto s = parlay::sequence<int>();
  ASSERT_EQ(s.size(), 0);
  ASSERT_TRUE(s.empty());
}

TEST(TestSequence, TestValueInitConstruct) {
  auto s = parlay::sequence<int>(100);
  ASSERT_EQ(s.size(), 100);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(s[i], 0);
  }
}

TEST(TestSequence, TestFillConstruct) {
  auto s = parlay::sequence<int>(100, 42);
  ASSERT_EQ(s.size(), 100);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 100; i++) {
    ASSERT_EQ(s[i], 42);
  }
}

TEST(TestSequence, TestInitializerListConstruct) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9,10};
  ASSERT_EQ(s.size(), 10);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 10; i++) {
    ASSERT_EQ(s[i], i+1);
  }
}

TEST(TestSequence, TestCopyConstructor) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_EQ(s.size(), 6);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>(s);
  for (size_t i = 0; i < 6; i++) {
    ASSERT_EQ(s[i], i+1);
    ASSERT_EQ(s[i], s2[i]);
  }
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestMoveConstructor) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_EQ(s.size(), 6);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>(std::move(s));
  ASSERT_TRUE(s.empty());
  ASSERT_FALSE(s2.empty());
  for (size_t i = 0; i < 6; i++) {
    ASSERT_EQ(s2[i], i+1);
  }
}

TEST(TestSequence, TestSmallConstruct) {
  auto s = parlay::sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 2; i++) {
    ASSERT_EQ(s[i], i+1);
  }
}

TEST(TestSequence, TestSmallCopy) {
  auto s = parlay::sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>(s);
  ASSERT_EQ(s, s2);
  ASSERT_EQ(s2.size(), 2);
  ASSERT_FALSE(s2.empty());
  for (size_t i = 0; i < 2; i++) {
    ASSERT_EQ(s2[i], i+1);
    ASSERT_EQ(s[i], s2[i]);
  }
}

TEST(TestSequence, TestCopyAssign) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_EQ(s.size(), 6);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>{};
  ASSERT_TRUE(s2.empty());
  s2 = s;
  ASSERT_FALSE(s.empty());
  ASSERT_FALSE(s2.empty());
  ASSERT_EQ(s, s2);
}

// Since SSO is disabled for non-trivial types,
// this should be the same as copying
TEST(TestSequence, TestSmallMove) {
  auto s = parlay::sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>(std::move(s));
  ASSERT_TRUE(s.empty());
  ASSERT_EQ(s2.size(), 2);
  ASSERT_FALSE(s2.empty());
  for (size_t i = 0; i < 2; i++) {
    ASSERT_EQ(s2[i], i+1);
  }
}

TEST(TestSequence, TestCopyAssignOther) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_EQ(s.size(), 6);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>{7,8,9};
  s2 = parlay::sequence<int>(s);
  for (size_t i = 0; i < 6; i++) {
    ASSERT_EQ(s[i], i+1);
    ASSERT_EQ(s[i], s2[i]);
  }
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestMoveAssign) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_EQ(s.size(), 6);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::sequence<int>{7,8,9};
  s2 = std::move(s);
  ASSERT_TRUE(s.empty());
  ASSERT_FALSE(s2.empty());
  for (size_t i = 0; i < 6; i++) {
    ASSERT_EQ(s2[i], i+1);
  }
}

TEST(TestSequence, TestInitializerListAssign) {
  auto s = parlay::sequence<int>{42,3,1};
  s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9,10};
  ASSERT_EQ(s.size(), 10);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 10; i++) {
    ASSERT_EQ(s[i], i+1);
  }
}

// SSO is disabled for non-trivial types
// so this should just do a heap allocation
TEST(TestSequence, TestSmallNonTrivial) {
  auto s = parlay::sequence<std::unique_ptr<int>>();
  ASSERT_TRUE(s.empty());
  s.push_back(std::make_unique<int>(5));
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(s.size(), 1);
  ASSERT_EQ(*s[0], 5);
}

TEST(TestSequence, TestLargeNonTrivial) {
  auto s = parlay::sequence<std::vector<int>>{
    std::vector<int>{1,2},
    std::vector<int>{4,5}
  };
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(s[0].size(), 2);
  ASSERT_EQ(s[1].size(), 2);
  ASSERT_EQ(s[0][0], 1);
  ASSERT_EQ(s[0][1], 2);
  ASSERT_EQ(s[1][0], 4);
  ASSERT_EQ(s[1][1], 5);
}

TEST(TestSequence, TestConvertFromRandomAccessRange) {
  auto v = std::vector<int>{1,2,3,4,5};
  auto s = parlay::to_sequence(v);
  ASSERT_EQ(v.size(), s.size());
  ASSERT_TRUE(std::equal(std::begin(v), std::end(v), std::begin(s)));
}

TEST(TestSequence, TestConvertFromForwardRange) {
  auto l = std::list<int>{1,2,3,4,5};
  auto s = parlay::to_sequence(l);
  ASSERT_EQ(l.size(), s.size());
  ASSERT_TRUE(std::equal(std::begin(l), std::end(l), std::begin(s)));
}

TEST(TestSequence, TestSwapSmall) {
  auto s1 = parlay::sequence<int>{1,2};
  auto s2 = parlay::sequence<int>{6,7};
  ASSERT_NE(s1, s2);
  std::swap(s1, s2);
  ASSERT_EQ(s1, (parlay::sequence<int>{6,7}));
  ASSERT_EQ(s2, (parlay::sequence<int>{1,2}));
}

TEST(TestSequence, TestSwapLarge) {
  auto s1 = parlay::sequence<int>{1,2,3,4,5};
  auto s2 = parlay::sequence<int>{6,7,8,9,10};
  ASSERT_NE(s1, s2);
  std::swap(s1, s2);
  ASSERT_EQ(s1, (parlay::sequence<int>{6,7,8,9,10}));
  ASSERT_EQ(s2, (parlay::sequence<int>{1,2,3,4,5}));
}

TEST(TestSequence, TestSubscript) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s[i], i+1);
    s[i] = i * 2;
    ASSERT_EQ(s[i], i * 2);
  }
}

TEST(TestSequence, TestSubscriptConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  const auto& sr = s;
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(sr[i], i+1);
  }
}

TEST(TestSequence, TestEmplace) {
  auto s = parlay::sequence<int>{1,2,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  ASSERT_FALSE(s.empty());
  s.emplace(s.begin() + 2, 3);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestEmplaceBack) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_FALSE(s.empty());
  s.emplace_back(6);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestEmplaceBackMove) {
  auto s = parlay::sequence<std::unique_ptr<int>>{};
  s.emplace_back(std::make_unique<int>(5));
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(*s[0], 5);
}

TEST(TestSequence, TestEmplaceBackNonTrivial) {
  auto s = parlay::sequence<std::vector<int>>{};
  s.emplace_back(5,5);
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(s[0], std::vector<int>(5,5));
}

TEST(TestSequence, TestPushBack) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6};
  ASSERT_FALSE(s.empty());
  s.push_back(6);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestPushBackMove) {
  auto s = parlay::sequence<std::unique_ptr<int>>{};
  s.push_back(std::make_unique<int>(5));
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(*s[0], 5);
}

TEST(TestSequence, TestAppend) {
  auto s1 = parlay::sequence<int>{1,2,3,4};
  auto s2 = parlay::sequence<int>{5,6,7,8};
  ASSERT_FALSE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(s2);
  ASSERT_EQ(s1.size(), 8);
  for (int i = 0; i < 8; i++) {
    ASSERT_EQ(s1[i], i+1);
  }
}

TEST(TestSequence, TestAppendIteratorRange) {
  auto s1 = parlay::sequence<int>{1,2,3,4};
  auto s2 = std::vector<int>{5,6,7,8};
  ASSERT_FALSE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(std::begin(s2), std::end(s2));
  ASSERT_EQ(s1.size(), 8);
  for (int i = 0; i < 8; i++) {
    ASSERT_EQ(s1[i], i+1);
  }
}

TEST(TestSequence, TestAppendValues) {
  auto s1 = parlay::sequence<int>{1,2,3,4};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,5,5,5,5};
  ASSERT_FALSE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(5, 5);
  ASSERT_EQ(s1.size(), 9);
  ASSERT_EQ(s1, s2);
}

TEST(TestSequence, TestAppendMove) {
  auto s1 = parlay::sequence<int>{1,2,3,4};
  auto s2 = parlay::sequence<int>{5,6,7,8};
  ASSERT_FALSE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(std::move(s2));
  ASSERT_EQ(s1.size(), 8);
  for (int i = 0; i < 8; i++) {
    ASSERT_EQ(s1[i], i+1);
  }
}

TEST(TestSequence, TestAppendMoveNonTrivial) {
  auto s1 = parlay::sequence<std::unique_ptr<int>>{};
  auto s2 = parlay::sequence<std::unique_ptr<int>>{};
  
  s1.emplace_back(std::make_unique<int>(5));
  s2.emplace_back(std::make_unique<int>(6));
  
  ASSERT_FALSE(s1.empty());
  ASSERT_FALSE(s2.empty());
  
  s1.append(std::move(s2));
  
  ASSERT_EQ(s1.size(), 2);
  ASSERT_TRUE(s2[0] == nullptr);
  ASSERT_EQ(*s1[0], 5);
  ASSERT_EQ(*s1[1], 6);
}
