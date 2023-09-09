#include <atomic>
#include <list>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include <parlay/alloc.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>
#include <parlay/type_traits.h>
#include <parlay/utilities.h>

// Sequences should be trivially relocatable provided that they
// have a trivially relocatable allocator
static_assert(parlay::is_trivially_relocatable_v<parlay::sequence<int, std::allocator<int>>>);
static_assert(parlay::is_trivially_relocatable_v<parlay::sequence<int, parlay::allocator<int>>>);
static_assert(parlay::is_trivially_relocatable_v<parlay::short_sequence<int, std::allocator<int>>>);
static_assert(parlay::is_trivially_relocatable_v<parlay::short_sequence<int, parlay::allocator<int>>>);

// With GNU packed structs, everything should fit into 16 bytes.
#if defined(__GNUC__) && !defined(__MINGW64__)
static_assert(sizeof(parlay::sequence<int>) <= 16);
static_assert(sizeof(parlay::short_sequence<int>) <= 16);
#endif

static_assert(alignof(parlay::sequence<int>) >= 8);


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
  auto s = parlay::short_sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  for (size_t i = 0; i < 2; i++) {
    ASSERT_EQ(s[i], i+1);
  }
}

TEST(TestSequence, TestSmallCopy) {
  auto s = parlay::short_sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::short_sequence<int>(s);
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
  auto s = parlay::short_sequence<int>{1,2};
  ASSERT_EQ(s.size(), 2);
  ASSERT_FALSE(s.empty());
  auto s2 = parlay::short_sequence<int>(std::move(s));
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
  auto s = parlay::short_sequence<std::unique_ptr<int>>();
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

TEST(TestSequence, TestConvertShortFromRandomAccessRange) {
  auto v = std::vector<int>{1,2,3,4,5};
  auto s = parlay::to_short_sequence(v);
  ASSERT_EQ(v.size(), s.size());
  ASSERT_TRUE(std::equal(std::begin(v), std::end(v), std::begin(s)));
}

TEST(TestSequence, TestConvertShortFromForwardRange) {
  auto l = std::list<int>{1,2,3,4,5};
  auto s = parlay::to_short_sequence(l);
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

TEST(TestSequence, TestAt) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s.at(i), i+1);
    s.at(i) = i * 2;
    ASSERT_EQ(s.at(i), i * 2);
  }
}

TEST(TestSequence, TestAtConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5};
  const auto& sr = s;
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(sr.at(i), i+1);
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

TEST(TestSequence, TestMoveAppendToEmpty) {
  parlay::sequence<int> s1{};
  auto s2 = parlay::sequence<int>{5,6,7,8};
  ASSERT_TRUE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(std::move(s2));
  ASSERT_EQ(s1.size(), 4);
  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(s1[i], 5+i);
  }
}

TEST(TestSequence, TestMoveAppendToEmptyAfterReserve) {
  parlay::sequence<int> s1{};
  s1.reserve(100);
  auto s2 = parlay::sequence<int>{5,6,7,8};
  ASSERT_TRUE(s1.empty());
  ASSERT_FALSE(s2.empty());
  s1.append(std::move(s2));
  ASSERT_EQ(s1.size(), 4);
  ASSERT_GE(s1.capacity(), size_t{100});
  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(s1[i], 5+i);
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
  ASSERT_EQ(s2[0], nullptr);
  ASSERT_NE(s1[0], nullptr);
  ASSERT_NE(s1[1], nullptr);
  ASSERT_EQ(*s1[0], 5);
  ASSERT_EQ(*s1[1], 6);
}

TEST(TestSequence, TestInsert) {
  auto s = parlay::sequence<int>{1,2,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 2, 3);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestInsertRef) {
  auto s = parlay::sequence<int>{1,2,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  ASSERT_FALSE(s.empty());
  int x = 3;
  s.insert(s.begin() + 2, x);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestInsertMove) {
  auto s = parlay::sequence<std::unique_ptr<int>>{};
  s.emplace_back(std::make_unique<int>(1));
  s.emplace_back(std::make_unique<int>(3));
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 1, std::make_unique<int>(2));
  ASSERT_EQ(*s[1], 2);
}

TEST(TestSequence, TestInsertCopies) {
  auto s = parlay::sequence<int>{1,2,4,5};
  auto s2 = parlay::sequence<int>{1,2,3,3,3,3,3,4,5};
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 2, 5, 3);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestInsertIteratorRange) {
  auto s = parlay::sequence<int>{1,2,8,9};
  auto s2 = parlay::sequence<int>{3,4,5,6,7};
  auto s3 = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 2, s2.begin(), s2.end());
  ASSERT_EQ(s, s3);
}

TEST(TestSequence, TestInsertRange) {
  auto s = parlay::sequence<int>{1,2,8,9};
  auto s2 = parlay::sequence<int>{3,4,5,6,7};
  auto s3 = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 2, s2);
  ASSERT_EQ(s, s3);
}

TEST(TestSequence, TestInsertInitializerList) {
  auto s = parlay::sequence<int>{1,2,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  ASSERT_FALSE(s.empty());
  s.insert(s.begin() + 2, {3,4,5,6,7});
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestErase) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,6,7,8,9};
  ASSERT_FALSE(s.empty());
  s.erase(s.begin() + 4);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestEraseIteratorRange) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,7,8,9};
  ASSERT_FALSE(s.empty());
  s.erase(s.begin() + 3, s.begin() + 6);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestPopBack) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6,7,8};
  ASSERT_FALSE(s.empty());
  s.pop_back();
  ASSERT_EQ(s.size(), 8);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestSize) {
  auto s = parlay::sequence<int>{};
  ASSERT_EQ(s.size(), 0);
  s.push_back(1);
  ASSERT_EQ(s.size(), 1);
  s.pop_back();
  ASSERT_EQ(s.size(), 0);
  s.append(5, 10);
  ASSERT_EQ(s.size(), 5);
  s.erase(s.begin() + 1, s.begin() + 3);
  ASSERT_EQ(s.size(), 3);
  s.insert(s.begin(), 10, 3);
  ASSERT_EQ(s.size(), 13);
}

TEST(TestSequence, TestClear) {
  auto s = parlay::sequence<int>{1,2,3};
  ASSERT_FALSE(s.empty());
  s.clear();
  ASSERT_TRUE(s.empty());
}

TEST(TestSequence, TestResizeUp) {
  auto s = parlay::sequence<int>{1,2,3};
  ASSERT_EQ(s.size(), 3);
  s.resize(10);
  ASSERT_EQ(s.size(), 10);
  for (int i = 3; i < 10; i++) {
    ASSERT_EQ(s[i], 0);
  }
}

TEST(TestSequence, TestResizeValue) {
  auto s = parlay::sequence<int>{1,2,3};
  ASSERT_EQ(s.size(), 3);
  s.resize(10, 42);
  ASSERT_EQ(s.size(), 10);
  for (int i = 3; i < 10; i++) {
    ASSERT_EQ(s[i], 42);
  }
}

TEST(TestSequence, TestResizeDown) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  ASSERT_FALSE(s.empty());
  s.resize(5);
  ASSERT_EQ(s.size(), 5);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestAssignIteratorRange) {
  auto s = parlay::sequence<int>{};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6};
  s.assign(s2.begin(), s2.end());
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestAssignRange) {
  auto s = parlay::sequence<int>{};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6};
  s.assign(s2);
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestAssignInitializerList) {
  auto s = parlay::sequence<int>{};
  auto s2 = parlay::sequence<int>{1,2,3,4,5,6};
  s.assign({1,2,3,4,5,6});
  ASSERT_EQ(s, s2);
}

TEST(TestSequence, TestAssignCopies) {
  auto s = parlay::sequence<int>{};
  s.assign(10, 42);
  ASSERT_EQ(s.size(), 10);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(s[i], 42);
  }
}

TEST(TestSequence, TestFront) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  ASSERT_EQ(s.front(), 1);
  s.front() = 42;
  ASSERT_EQ(s.front(), 42);
  ASSERT_EQ(s[0], 42);
}

TEST(TestSequence, TestFrontConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  const auto& sr = s;
  ASSERT_EQ(sr.front(), 1);
}

TEST(TestSequence, TestBack) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  ASSERT_EQ(s.back(), 9);
  s.back() = 42;
  ASSERT_EQ(s.back(), 42);
  ASSERT_EQ(s[8], 42);
}

TEST(TestSequence, TestBackConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  const auto& sr = s;
  ASSERT_EQ(sr.back(), 9);
}

TEST(TestSequence, TestHead) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  auto h = s.head(5);
  ASSERT_EQ(h.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), h.begin()));
  for (auto& x : h) { x++; }
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s[i], s2[i]+1);
  }
}

TEST(TestSequence, TestCut) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{4,5,6,7};
  auto ss = s.cut(3,7);
  ASSERT_EQ(ss.size(), 4);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), ss.begin()));
  for (auto& x : ss) { x++; }
  for (int i = 0; i < 4; i++) {
    ASSERT_EQ(s[i+3], s2[i]+1);
  }
}

TEST(TestSequence, TestCutConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{4,5,6,7};
  auto ss = s.cut(3,7);
  ASSERT_EQ(ss.size(), 4);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), ss.begin()));
}

TEST(TestSequence, TestSubstrToEnd) {
  const auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  const auto s2 = parlay::sequence<int>{4,5,6,7,8,9};
  auto ss = s.substr(3);
  ASSERT_EQ(ss.size(), 6);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), ss.begin()));
}

TEST(TestSequence, TestSubstr) {
  const auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  const auto s2 = parlay::sequence<int>{4,5,6,7};
  auto ss = s.substr(3,4);
  ASSERT_EQ(ss.size(), 4);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), ss.begin()));
}

TEST(TestSequence, TestSubseq) {
  const auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  const auto s2 = parlay::sequence<int>{4,5,6,7};
  auto ss = s.subseq(3,7);
  ASSERT_EQ(ss.size(), 4);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), ss.begin()));
}

TEST(TestSequence, TestHeadConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  auto h = s.head(5);
  ASSERT_EQ(h.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), h.begin()));
}

TEST(TestSequence, TestTail) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.tail(5);
  ASSERT_EQ(t.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), t.begin()));
  for (auto& x : t) { x++; }
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s[i+4], s2[i]+1);
  }
}

TEST(TestSequence, TestTailConst) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.tail(5);
  ASSERT_EQ(t.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), t.begin()));
}

TEST(TestSequence, TestHead2) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  auto h = s.head(s.begin() + 5);
  ASSERT_EQ(h.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), h.begin()));
  for (auto& x : h) { x++; }
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s[i], s2[i]+1);
  }
}

TEST(TestSequence, TestHeadConst2) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  auto h = s.head(s.begin() + 5);
  ASSERT_EQ(h.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), h.begin()));
}

TEST(TestSequence, TestTail2) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.tail(s.end() - 5);
  ASSERT_EQ(t.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), t.begin()));
  for (auto& x : t) { x++; }
  for (int i = 0; i < 5; i++) {
    ASSERT_EQ(s[i+4], s2[i]+1);
  }
}

TEST(TestSequence, TestTailConst2) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.tail(s.end() - 5);
  ASSERT_EQ(t.size(), 5);
  ASSERT_TRUE(std::equal(s2.begin(), s2.end(), t.begin()));
}

TEST(TestSequence, TestPopTail) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.pop_tail(5);
  ASSERT_EQ(s.size(), 4);
  ASSERT_EQ(t.size(), 5);
  ASSERT_EQ(s2, t);
}

TEST(TestSequence, TestPopTail2) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{5,6,7,8,9};
  auto t = s.pop_tail(s.end() - 5);
  ASSERT_EQ(s.size(), 4);
  ASSERT_EQ(t.size(), 5);
  ASSERT_EQ(s2, t);
}

TEST(TestSequence, TestEqual) {
  auto s1 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  auto s2 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  ASSERT_EQ(s1, s2);
}

TEST(TestSequence, TestNotEqual) {
  // All different
  auto s1 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  auto s2 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 2;
  });
  ASSERT_NE(s1, s2);
}

TEST(TestSequence, TestNotEqual2) {
  // Equal up until the last element
  auto s1 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  auto s2 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  s2.back() = 0;
  ASSERT_NE(s1, s2);
}

TEST(TestSequence, TestNotEqual3) {
  // Different lengths
  auto s1 = parlay::sequence<size_t>::from_function(1000000, [](size_t i) {
    return 2*i + 1;
  });
  auto s2 = parlay::sequence<size_t>::from_function(999999, [](size_t i) {
    return 2*i + 1;
  });
  ASSERT_NE(s1, s2);
}

TEST(TestSequence, TestCapacity) {
  auto s = parlay::sequence<int>(2000);
  ASSERT_GE(s.capacity(), size_t{2000});
  auto s2 = parlay::sequence<int>();
  s2.reserve(2000);
  ASSERT_GE(s2.capacity(), size_t{2000});
}

TEST(TestSequence, TestReserve) {  
  auto s = parlay::sequence<int>();
  s.reserve(1000);
  auto cap = s.capacity();
  ASSERT_GE(cap, size_t{1000});
  for (int i = 0; i < 1000; i++) {
    s.push_back(i);
  }
  ASSERT_EQ(s.size(), 1000);
  ASSERT_EQ(s.capacity(), cap);
}

TEST(TestSequence, TestSequenceOfAtomic) {
  parlay::sequence<std::atomic<int>> s(10000);
  for (int i = 0; i < 10000; i++) {
    s[i].store(i);
  }
}

struct NotDefaultConstructible {
  int x;
  NotDefaultConstructible(int _x) : x(_x) { }
};

TEST(TestSequence, TestNonDefaultConstructibleType) {
  static_assert(!std::is_default_constructible_v<NotDefaultConstructible>);
  parlay::sequence<NotDefaultConstructible> s;
  for (int i = 0; i < 100000; i++) {
    s.emplace_back(i);
  }
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(s[i].x, i);
  }
}

TEST(TestSequence, TestCopyElisionFromFunction) {
  struct foo {
    std::atomic<int> x, y;
  };
  static_assert(!std::is_copy_constructible_v<foo>);
  static_assert(!std::is_move_constructible_v<foo>);

  // foo is not copy or move constructible, so this will only
  // work if copy elision succeeds in directly constructing
  // the foo straight into the sequence
  auto s = parlay::sequence<foo>::from_function(100000, [](int i) {
    return foo{i, i+1};
  });

  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i].x.load(), i);
    ASSERT_EQ(s[i].y.load(), i+1);
  }
}

struct NonStandardLayout {
  int x;
  NonStandardLayout(int _x) : x(_x) { }
  virtual ~NonStandardLayout() = default;
};

TEST(TestSequence, TestNonStandardLayout) {
  static_assert(!std::is_standard_layout_v<NonStandardLayout>);
  parlay::sequence<NotDefaultConstructible> s;
  for (int i = 0; i < 100000; i++) {
    s.emplace_back(i);
  }
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(s[i].x, i);
  }
}

TEST(TestSequence, TestOtherAllocator) {
  parlay::sequence<int, std::allocator<double>> s;
  for (int i = 0; i < 100000; i++) {
    s.push_back(i);
  }
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(s[i], i);
  }
}

TEST(TestSequence, TestGetAllocator) {
  parlay::sequence<int, std::allocator<int>> s;
  auto alloc = s.get_allocator();
  ASSERT_EQ(alloc, std::allocator<int>());
}

TEST(TestSequence, TestLessThan) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto s2 = parlay::sequence<int>{1,2,3,4,5};
  auto s3 = parlay::sequence<int>{1,2,3,4,5,6,7,8,10};
  auto s4 = parlay::sequence<int>{1,2,3,4,6};

  ASSERT_LT(s2, s);
  ASSERT_LT(s, s3);
  ASSERT_LT(s, s4);
}

#if defined(PARLAY_EXCEPTIONS_ENABLED)

TEST(TestSequence, TestAtThrow) {
  auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  EXPECT_THROW({ s.at(9); }, std::out_of_range);
}

TEST(TestSequence, TestAtThrowConst) {
  const auto s = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  EXPECT_THROW({ s.at(9); }, std::out_of_range);
}

#else

TEST(TestSequenceDeathTest, TestAt) {
  ASSERT_EXIT({
    auto s = parlay::sequence<int>({1,2,3,4,5,6,7,8,9});
    s.at(9);
  }, testing::KilledBySignal(SIGABRT), "sequence access out of bounds");
}

TEST(TestSequenceDeathTest, TestAtConst) {
  ASSERT_EXIT({
    const auto s = parlay::sequence<int>({1,2,3,4,5,6,7,8,9});
    s.at(9);
  }, testing::KilledBySignal(SIGABRT), "sequence access out of bounds");
}

#endif  // defined(PARLAY_EXCEPTIONS_ENABLED)

// TODO: More thorough tests with custom allocators
// to validate allocator usage.
