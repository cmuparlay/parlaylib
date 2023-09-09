#include "gtest/gtest.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include <parlay/delayed_sequence.h>

TEST(TestDelayedSequence, TestConstruction) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  ASSERT_EQ(s.size(), 100000);
}

struct MyFunctor {
  std::unique_ptr<int> f;
  explicit MyFunctor(int _f) : f(std::make_unique<int>(_f)) { }
  MyFunctor(const MyFunctor& other) : f(std::make_unique<int>(*(other.f))) {}
  MyFunctor& operator=(const MyFunctor& other) {
    f = std::make_unique<int>(*(other.f));
    return *this;
  }
  MyFunctor(MyFunctor&& other) : f(std::move(other.f)) { }
  MyFunctor& operator=(MyFunctor&& other) {
    f = std::move(other.f);
    return *this;
  }
  int operator()(int i) const { return *f*i; }
};

TEST(TestDelayedSequence, TestCopyConstruct) {
  auto s = parlay::delayed_seq<int>(100000, MyFunctor(1));
  auto s2 = s;
  ASSERT_EQ(s2.size(), s.size());
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(s2)));
}

TEST(TestDelayedSequence, TestMoveConstruct) {
  auto s = parlay::delayed_seq<int>(100000, MyFunctor(1));
  auto s2 = std::move(s);
  ASSERT_EQ(s2.size(), 100000);
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(s2[i], i);
  }
}

TEST(TestDelayedSequence, TestCopyAssign) {
  auto s = parlay::delayed_seq<int>(100000, MyFunctor(1));
  ASSERT_EQ(s.size(), 100000);
  for (size_t i = 0; i < 10000; i++) {
    ASSERT_EQ(s[i], i);
  }
  s = parlay::delayed_seq<int>(200000, MyFunctor(2));
  ASSERT_EQ(s.size(), 200000);
  for (size_t i = 0; i < 20000; i++) {
    ASSERT_EQ(s[i], i*2);
  }
}

TEST(TestDelayedSequence, TestMoveAssign) {
  auto s = parlay::delayed_seq<int>(100000, MyFunctor(1));
  for (size_t i = 0; i < 10000; i++) {
    ASSERT_EQ(s[i], i);
  }
  auto s2 = parlay::delayed_seq<int>(200000, MyFunctor(2));
  s = std::move(s2);
  ASSERT_EQ(s.size(), 200000);
  for (size_t i = 0; i < 20000; i++) {
    ASSERT_EQ(s[i], i*2);
  }
}

// I don't know why you would ever want to do this but
// hey, its possible
TEST(TestDelayedSequence, TestLambdaCapture) {
  auto v = std::vector<int>(100000);
  std::iota(std::begin(v), std::end(v), 0);
  auto s = parlay::delayed_seq<int>(100000, [v = std::move(v)](size_t i) {
    return v[i];
  });
  ASSERT_EQ(s.size(), 100000);
  for (size_t i = 0; i < 10000; i++) {
    ASSERT_EQ(s[i], i);
  }
}

TEST(TestDelayedSequence, TestAsInputIterator) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  std::vector<int> v;
  std::copy(std::begin(s), std::end(s), std::back_inserter(v));
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(v)));
}

TEST(TestDelayedSequence, TestForwardIterator) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  size_t i = 0;
  for (auto it = s.begin(); it != s.end(); it++) {
    ASSERT_EQ(*it, i++);
  }
  ASSERT_EQ(i, 100000);
}

TEST(TestDelayedSequence, TestBackwardIterator) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  size_t i = 100000;
  for (auto it = s.end(); it != s.begin(); it--) {
    ASSERT_EQ(*(it-1), --i);
  }
  ASSERT_EQ(i, 0);
}

TEST(TestDelayedSequence, TestAsReverseIterator) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  std::vector<int> v;
  std::copy(std::rbegin(s), std::rend(s), std::back_inserter(v));
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::rbegin(v)));
}

TEST(TestDelayedSequence, TestAsRandomAccess) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  auto found = std::binary_search(std::begin(s), std::end(s), 49998);
  ASSERT_TRUE(found);
  auto it = std::lower_bound(std::begin(s), std::end(s), 49998);
  ASSERT_NE(it, std::end(s));
  ASSERT_EQ(*it, 49998);
}

TEST(TestDelayedSequence, TestSubscript) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(i, s[i]);
  }
}

TEST(TestDelayedSequence, TestAt) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(i, s.at(i));
  }
}

TEST(TestDelayedSequence, TestFront) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  ASSERT_EQ(s.front(), 0);
}

TEST(TestDelayedSequence, TestBack) {
  auto s = parlay::delayed_seq<int>(100000, [](int i) -> int { return i; });
  ASSERT_EQ(s.back(), 99999);
}

// Delayed sequences can be used to return references, so that
// they will not make copies of the things they refer to.
TEST(TestDelayedSequence, TestDelayedSequenceOfReferences) {
  std::vector<std::unique_ptr<int>> v;
  for (int i = 0; i < 100000; i++) {
    v.emplace_back(std::make_unique<int>(i));
  }
  auto s = parlay::delayed_seq<const std::unique_ptr<int>&>(100000, [&](size_t i) -> const std::unique_ptr<int>& {
    return v[i];
  });
  for (int i = 0; i < 100000; i++) {
    const auto& si = s[i];
    ASSERT_EQ(*v[i], *si);
  }
}

// Delayed sequences can be used to return references, and
// they can even be mutable references, so we can modify
// the underlying source!
TEST(TestDelayedSequence, TestDelayedSequenceOfMutableReferences) {
  std::vector<std::unique_ptr<int>> v;
  for (int i = 0; i < 100000; i++) {
    v.emplace_back(std::make_unique<int>(i));
  }
  auto s = parlay::delayed_seq<int&>(100000, [&](size_t i) -> int& {
    return *v[i];
  });
  for (int i = 0; i < 100000; i++) {
    s[i]++;
    ASSERT_EQ(*v[i], i+1);
  }
}

#if defined(PARLAY_EXCEPTIONS_ENABLED)

TEST(TestDelayedSequence, TestAtThrow) {
  auto s = parlay::delayed_seq<int>(9, [](int i) -> int { return i; });
  EXPECT_THROW({ s.at(9); }, std::out_of_range);
}

TEST(TestDelayedSequence, TestAtThrowConst) {
  const auto s = parlay::delayed_seq<int>(9, [](int i) -> int { return i; });
  EXPECT_THROW({ s.at(9); }, std::out_of_range);
}

#else

TEST(TestDelayedSequenceDeathTest, TestAt) {
  ASSERT_EXIT({
    auto s = parlay::delayed_seq<int>(9, [](int i) -> int { return i; });
    s.at(9);
  }, testing::KilledBySignal(SIGABRT), "Delayed sequence access out of range");
}

TEST(TestDelayedSequenceDeathTest, TestAtConst) {
  ASSERT_EXIT({
    const auto s = parlay::delayed_seq<int>(9, [](int i) -> int { return i; });
    s.at(9);
  }, testing::KilledBySignal(SIGABRT), "Delayed sequence access out of range");
}

#endif  // defined(PARLAY_EXCEPTIONS_ENABLED)
