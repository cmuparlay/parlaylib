#include "gtest/gtest.h"

#include <numeric>
#include <type_traits>
#include <vector>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/range.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

// Check that flattened ranges are copyable and movable
using ssi = parlay::sequence<parlay::sequence<int>>;
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<ssi>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<ssi>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<ssi&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<ssi&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<ssi>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<ssi>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<ssi&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<ssi&>()))>);

struct dsi { parlay::sequence<int> operator()(size_t) const; };
using dssi = decltype(parlay::delayed_tabulate(0, std::declval<dsi>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<dssi>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<dssi>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<dssi&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<dssi&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<dssi>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<dssi>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<dssi&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<dssi&>()))>);

using bdssi = decltype(parlay::block_iterable_wrapper(std::declval<ssi>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<bdssi>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<bdssi>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::flatten(std::declval<bdssi&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::flatten(std::declval<bdssi&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<bdssi>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<bdssi>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::flatten(std::declval<bdssi&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::flatten(std::declval<bdssi&>()))>);


// ---------------------------------------------------------------------------------------
//                                     RAD VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedFlatten, TestRadFlattenEmpty) {
  const parlay::sequence<parlay::sequence<int>> seq;
  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

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
  ASSERT_EQ(f.get_num_blocks(), 0);

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

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenConst) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  const auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenConstAndNonConst) {

  parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  const auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenBalancedOwning) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(std::move(seq));

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenUnevenLast) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2001, [](size_t) {
    return parlay::tabulate(2001, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2001*2001);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2001);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenToSeq) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto s = parlay::delayed::to_sequence(f);
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(s[i], i % 2500);
  }
}

TEST(TestDelayedFlatten, TestRadFlattenManySmall) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(50, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 50);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenFewLarge) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(50, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenMutable) {

  parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    (*it) += 10000;
    ASSERT_EQ(seq[i/2500][i%2500], *it);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestRadFlattenWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> seq = parlay::tabulate(2500, [](size_t i) {
    if (i % 3 == 2) return parlay::tabulate(2500, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2082500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
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
  auto seq = parlay::delayed_tabulate(2500, [](size_t) { return parlay::iota(2500); });

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], i % 2500);
  }
}

TEST(TestDelayedFlatten, TestRadFlattenRvalueReferences) {
  std::vector<std::vector<std::vector<int>>> s = {
      {{0,1,2}, {3,4,5}, {6,7,8}},
      {{3,4,5}, {6,7,8}, {9,10,11}},
      {{12,13,14}, {15,16,17}, {18,19,20}}
  };

  auto d = parlay::tabulate(3, [&s](size_t i) {
    return parlay::delayed_tabulate(3, [&s, i](size_t j) -> std::vector<int>&& {
      return std::move(s[i][j]);
    });
  });

  auto f = parlay::delayed::flatten(d);
  static_assert(std::is_same_v<parlay::range_reference_type_t<decltype(f)>, std::vector<int>&&>);
  ASSERT_EQ(f.size(), 9);

  auto seq = parlay::delayed::to_sequence<std::vector<int>>(f);
  ASSERT_EQ(seq.size(), 9);

  for (size_t i = 0; i < seq.size(); i++) {
    ASSERT_EQ(seq[i].size(), 3);
  }

  // If the input was moved from, they should all be empty
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      ASSERT_TRUE(s[i][j].empty());
    }
  }
}

TEST(TestDelayedFlatten, TestRadFlattenNoConst) {

  auto seq = parlay::NestedNonConstRange(2500);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

// ---------------------------------------------------------------------------------------
//                                     BID VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedFlatten, TestBidFlattenEmpty) {
  const parlay::sequence<parlay::sequence<int>> x;
  auto seq = parlay::block_iterable_wrapper(x);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFlatten, TestBidFlattenAllEmpty) {
  const parlay::sequence<parlay::sequence<int>> x = parlay::tabulate(100000, [](size_t) {
    return parlay::sequence<int>{};
  });
  auto seq = parlay::block_iterable_wrapper(x);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFlatten, TestBidFlattenTiny) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(10, [](size_t) {
    return parlay::tabulate(10, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 100);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 10);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenConst) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  const auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenNonConstAndConst) {

  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  const auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenBalanced) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenBalancedOwning) {

  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(std::move(s));

  auto f = parlay::delayed::flatten(std::move(seq));

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenUnevenLast) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(2001, [](size_t) {
    return parlay::tabulate(2001, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2001*2001);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2001);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenToSeq) {

  std::vector<std::vector<int>> ss(2500);
  for (size_t i = 0; i < 2500; i++) {
    ss[i] = std::vector<int>(2500);
    std::iota(std::begin(ss[i]), std::end(ss[i]), i * 2500);
  }

  const auto s = ss;

  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto seqd = parlay::delayed::to_sequence(f);
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(seqd[i], i);
  }
}

TEST(TestDelayedFlatten, TestBidFlattenManySmall) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t) {
    return parlay::tabulate(50, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 50);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenFewLarge) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(50, [](size_t) {
    return parlay::tabulate(5000, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 5000);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenMutable) {

  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(2500, [](size_t) {
    return parlay::tabulate(2500, [](size_t i) -> int { return i; });
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    (*it) += 10000;
    ASSERT_EQ(s[i/2500][i%2500], *it);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(2500, [](size_t i) {
    if (i % 3 == 2) return parlay::tabulate(2500, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2082500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenManySmallWithEmpty) {

  const parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(5000, [](size_t i) {
    if (i % 10 == 9) return parlay::tabulate(50, [](size_t i) -> int { return i; });
    else return parlay::sequence<int>{};
  });
  auto seq = parlay::block_iterable_wrapper(s);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 25000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 50);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenTemporaries) {
  auto x = parlay::delayed_tabulate(2500, [](size_t) { return parlay::iota(2500); });
  auto seq = parlay::block_iterable_wrapper(x);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], i % 2500);
  }
}

TEST(TestDelayedFlatten, TestBidFlattenRvalueReferences) {
  std::vector<std::vector<std::vector<int>>> s = {
      {{0,1,2}, {3,4,5}, {6,7,8}},
      {{3,4,5}, {6,7,8}, {9,10,11}},
      {{12,13,14}, {15,16,17}, {18,19,20}}
  };

  auto d = parlay::tabulate(3, [&s](size_t i) {
    return parlay::delayed_tabulate(3, [&s, i](size_t j) -> std::vector<int>&& {
      return std::move(s[i][j]);
    });
  });

  auto f = parlay::delayed::flatten(parlay::block_iterable_wrapper(d));
  static_assert(std::is_same_v<parlay::range_reference_type_t<decltype(f)>, std::vector<int>&&>);
  ASSERT_EQ(f.size(), 9);

  auto seq = parlay::delayed::to_sequence<std::vector<int>>(f);
  ASSERT_EQ(seq.size(), 9);

  for (size_t i = 0; i < seq.size(); i++) {
    ASSERT_EQ(seq[i].size(), 3);
  }

  // If the input was moved from, they should all be empty
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      ASSERT_TRUE(s[i][j].empty());
    }
  }
}

TEST(TestDelayedFlatten, TestBidFlattenNoConst) {
  auto x = parlay::NestedNonConstRange(2500);
  auto seq = parlay::block_iterable_wrapper(x);

  auto f = parlay::delayed::flatten(seq);

  ASSERT_EQ(f.size(), 2500*2500);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 2500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenCopyConstruct) {
  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });
  // Create a delayed flatten, then copy-construct and return the copy. the original should be
  // destroyed so if we accidentally take iterators from the old sequence they will dangle
  auto f = [&]() {
    auto f = parlay::delayed::flatten(parlay::block_iterable_wrapper(std::move(s)));
    auto f2 = f;
    return f2;
  }();

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenCopyAssign) {
  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });
  // Create a delayed flatten, then copy-assign and return the copy. the original should be
  // destroyed so if we accidentally take iterators from the old sequence they will dangle
  auto f = [&]() {
    auto f = parlay::delayed::flatten(parlay::block_iterable_wrapper(std::move(s)));
    auto f2 = f;
    f2 = f;
    return f2;
  }();

  ASSERT_EQ(f.size(), 250000);

  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, i % 500);
    ++it;
  }
  ASSERT_EQ(it, f.end());
}

TEST(TestDelayedFlatten, TestBidFlattenSwap) {
  parlay::sequence<parlay::sequence<int>> s = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return i; });
  });

  parlay::sequence<parlay::sequence<int>> s2 = parlay::tabulate(500, [](size_t) {
    return parlay::tabulate(500, [](size_t i) -> int { return 500+i; });
  });

  // Create a delayed flatten, then copy-assign and return the copy. the original should be
  // destroyed so if we accidentally take iterators from the old sequence they will dangle
  auto f = parlay::delayed::flatten(parlay::block_iterable_wrapper(std::move(s)));
  auto f2 = parlay::delayed::flatten(parlay::block_iterable_wrapper(std::move(s2)));

  ASSERT_EQ(f.size(), 250000);
  ASSERT_EQ(f2.size(), 250000);

  using std::swap;
  swap(f, f2);

  auto it = f.begin();
  auto it2 = f2.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, 500 + i % 500);
    ASSERT_EQ(*it2, i % 500);
    ++it;
    ++it2;
  }
  ASSERT_EQ(it, f.end());
}
