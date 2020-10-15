#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/slice.h>

#include <parlay/internal/merge_sort.h>

#include "sorting_utils.h"

TEST(TestMergeSort, TestMergeSort) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::merge_sort(make_slice(s), std::less<long long>());
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestMergeSort, TestMergeSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::merge_sort(make_slice(s), std::greater<long long>());
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestMergeSort, TestStableSort) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::internal::merge_sort(make_slice(s), std::less<UnstablePair>());
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestMergeSort, TestStableSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::internal::merge_sort(make_slice(s), std::greater<UnstablePair>());
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestMergeSort, TestSortInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}


TEST(TestMergeSort, TestSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::greater<long long>());
  std::sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestMergeSort, TestStableSortInplace) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::less<UnstablePair>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestMergeSort, TestStableSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::greater<UnstablePair>());
  std::stable_sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestMergeSort, TestMergeSortUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::less<UncopyableThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestMergeSort, TestMergeSortSelfReferential) {
  auto s = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(make_slice(s), std::less<SelfReferentialThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestMergeSort, TestSortNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::merge_sort_inplace(parlay::make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}
