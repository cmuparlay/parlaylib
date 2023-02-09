#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <tuple>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "sorting_utils.h"

TEST(TestSortingPrimitives, TestSort) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::sort(s);
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::sort(s, std::greater<long long>());
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestSortingPrimitives, TestStableSort) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::stable_sort(s);
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestStableSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::stable_sort(s, std::greater<UnstablePair>());
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestSortingPrimitives, TestSortInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::sort_inplace(s);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::sort_inplace(s, std::greater<long long>());
  std::sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestSortingPrimitives, TestStableSortInplace) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::stable_sort_inplace(s);
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::stable_sort_inplace(s, std::greater<UnstablePair>());
  std::stable_sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestSortingPrimitives, TestSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::sort_inplace(s, std::less<UncopyableThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::stable_sort_inplace(s, std::less<UncopyableThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::sort_inplace(s, std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::stable_sort_inplace(s, std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestIntegerSort) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::integer_sort(s);
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestIntegerSortInplace) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::integer_sort_inplace(s);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestIntegerSortCustomKey) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto sorted = parlay::integer_sort(s, [](const auto& x) -> unsigned long long {
    return x.x;
  });
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestStableIntegerSort) {
  auto s = parlay::tabulate(1000000, [](unsigned int i) {
    return std::make_pair(i % 10, i);
  });
  auto sorted = parlay::stable_integer_sort(s, [](const auto& p) { return p.first; });
  ASSERT_EQ(sorted.size(), s.size());
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestIntegerSortInplaceCustomKey) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::integer_sort_inplace(s, [](const auto& x) -> unsigned long long {
    return x.x;
  });
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableIntegerSortInplace) {
  auto s = parlay::tabulate(1000000, [](unsigned int i) {
    return std::make_pair(i % 10, i);
  });
  parlay::stable_integer_sort_inplace(s, [](const auto& p) { return p.first; });
  ASSERT_EQ(s.size(), 1000000);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestIntegerSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(100000-i);
  });
  auto s2 = parlay::tabulate(100000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(100000-i);
  });
  ASSERT_EQ(s, s2);
  parlay::integer_sort_inplace(s, [](const auto& a) -> unsigned int { return a.x; });
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableIntegerSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(100000-i);
  });
  auto s2 = parlay::tabulate(100000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(100000-i);
  });
  ASSERT_EQ(s, s2);
  parlay::stable_integer_sort_inplace(s, [](const auto& a) -> unsigned int { return a.x; });
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestIntegerSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<unsigned long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::integer_sort_inplace(s);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestStableIntegerSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<unsigned long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::stable_integer_sort_inplace(s, [](auto x) { return x; });
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestCountingSort) {
  const size_t num_buckets = size_t{1} << 16;
  auto s = parlay::tabulate(100000, [num_buckets](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % num_buckets;
  });

  auto sorted = parlay::counting_sort(parlay::make_slice(s), num_buckets).first;
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestCountingSortUnstable) {
  const size_t num_buckets = size_t{1} << 16;
  auto s = parlay::tabulate(100000, [num_buckets](long long i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % num_buckets;
    x.y = 0;
    return x;
  });

  auto get_key = [](auto&& x) -> size_t { return std::forward<decltype(x)>(x).x; };
  auto sorted = parlay::counting_sort(s, num_buckets, get_key).first;

  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestCountingSortInplace) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 16);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::counting_sort_inplace(s, 1 << 16);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestCountingSortCustomKey) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto sorted = parlay::counting_sort(s, 1 << 10, [](const auto& x) -> size_t { return x.x; }).first;
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestCountingSortInplaceCustomKey) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::counting_sort_inplace(s, 1 << 10, [](const auto& x) -> size_t { return x.x; });
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestCountingSortInplaceUncopyable) {
  auto s = parlay::tabulate(10000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(9999-i);
  });
  auto s2 = parlay::tabulate(10000, [](unsigned int i) -> UncopyableThing {
    return UncopyableThing(9999-i);
  });
  ASSERT_EQ(s, s2);
  parlay::counting_sort_inplace(s, 10000, [](const auto& a) -> unsigned int { return a.x; });
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}


TEST(TestSortingPrimitives, TestCountingSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) {
    return (50021 * i + 61) % (1 << 16);
  });
  auto s = std::deque<unsigned long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::counting_sort_inplace(s, 1 << 16);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSortingPrimitives, TestCountingSortByKeys) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto key_val_pairs = parlay::delayed_map(s, [](UnstablePair x) {
    return std::make_pair((unsigned int)x.x, x);
  });
  auto sorted = parlay::counting_sort_by_keys(key_val_pairs, 1 << 10).first;
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSortingPrimitives, TestCountingSortByKeysWithTuples) {
  auto s = parlay::tabulate(100000, [](unsigned int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto key_val_pairs = parlay::delayed_map(s, [](UnstablePair x) {
    return std::make_tuple((unsigned int)x.x, x);
  });
  auto sorted = parlay::counting_sort_by_keys(key_val_pairs, 1 << 10).first;
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

