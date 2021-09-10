#include "gtest/gtest.h"

#include <string>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

TEST(TestDelayedZip, TestRadZipSimple) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  parlay::sequence<int> b = {2,3,4,5,6,7,8,9,10,11};
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x+1);
  }
}

TEST(TestDelayedZip, TestRadZipReference) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  parlay::sequence<int> b = {2,3,4,5,6,7,8,9,10,11};
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    x--;
    y++;
  }

  for (size_t i = 0; i < a.size(); i++) {
    ASSERT_EQ(a[i] + 3, b[i]);
  }
}

TEST(TestDelayedZip, TestRadZipStrings) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });  // Pad to make strings not SSO
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}

TEST(TestDelayedZip, TestRadZipCopyByValue) {
  parlay::sequence<std::tuple<int,std::string>> res;

  // After this block, b goes out of scope, so if we accidentally
  // keep references instead of making copies then bad things
  // will happen!
  {
    parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });
    res = parlay::to_sequence(parlay::delayed::zip(a,b));
  }

  for (auto [x,y] : res) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}

TEST(TestDelayedZip, TestRadZipUncopyable) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}

TEST(TestDelayedZip, TestRadZipWithDelayed) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, y);
  }
}

TEST(TestDelayedZip, TestRadZipWithTemporary) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  auto zipped = parlay::delayed::zip(a, parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; }));
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, y);
  }
}

TEST(TestDelayedZip, TestRadZipWithDelayedUncopyable) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::delayed_map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}
