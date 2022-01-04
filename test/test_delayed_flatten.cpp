#include "gtest/gtest.h"

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

TEST(TestDelayedFlatten, TestRadFlattenEmpty) {
  const parlay::sequence<parlay::sequence<int>> seq;
  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFlatten, TestRadFlattenAllEmpty) {
  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(100000, [](size_t) {
    return parlay::sequence<int>{};
  });
  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFlatten, TestRadFlattenTiny) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(10, [](size_t) {
    return parlay::tabulate(10, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 100);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 10);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenBalanced) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenBalancedMove) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(std::move(seq));

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenUnevenLast) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(4001, [](size_t) {
    return parlay::tabulate(4001, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 16008001);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 4001);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenToSeq) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto s = parlay::delayed::to_sequence(f);
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(s[i], i % 5000);
  }
}

TEST(TestDelayedFlatten, TestRadFlattenManySmall) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(50000, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
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
  for (size_t i = 0; i < f.size(); i++) {
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
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    (*it) += 10000;
    ASSERT_EQ(seq[i/5000][i%5000], *it);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t i) {
    if (i % 3 == 2) return parlay::tabulate(5000, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 8330000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenManySmallWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(50000, [](size_t i) {
    if (i % 10 == 9) return parlay::tabulate(500, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenTemporaries) {
  auto seq = parlay::delayed_tabulate(5000, [](size_t) { return parlay::iota(5000); });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenTiny) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(10, [](size_t) {
    return parlay::tabulate(10, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 100);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 10);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenBalancedRef) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenBalancedMove) {

  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(std::move(s));

  auto f = parlay::delayed::flatten(std::move(seq));

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenUnevenLast) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(4001, [](size_t) {
    return parlay::tabulate(4001, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 16008001);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 4001);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenToSeq) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto seqd = parlay::delayed::to_sequence(f);
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(seqd[i], i % 5000);
  }
}

TEST(TestDelayedFlatten, TestBidFlattenManySmall) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(50000, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenFewLarge) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(50000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 50000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenMutable) {

  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    (*it) += 10000;
    ASSERT_EQ(s[i/5000][i%5000], *it);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t i) {
    if (i % 3 == 2) return parlay::tabulate(5000, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 8330000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenManySmallWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(50000, [](size_t i) {
    if (i % 10 == 9) return parlay::tabulate(500, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });
  auto seq = parlay::internal::delayed::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}