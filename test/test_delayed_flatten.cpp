#include "gtest/gtest.h"

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

TEST(TestDelayedFlatten, TestRadFlattenBalanced) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < 25000000; i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }

  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenManySmall) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(50000, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < 25000000; i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }

  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenFewLarge) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(50000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < 25000000; i++) {
    ASSERT_EQ(*it, i % 50000);
    ++it;
  }

  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenMutable) {

  parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < 25000000; i++) {
    ASSERT_EQ(*it, i % 5000);
    (*it) += 10000;
    ASSERT_EQ(seq[i/5000][i%5000], *it);
    ++it;
  }
}
