#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/slice.h>
#include <parlay/type_traits.h>

#include <parlay/internal/sample_sort.h>

#include "sorting_utils.h"

TEST(TestSampleSort, TestSort) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::sample_sort(parlay::make_slice(s), std::less<long long>());
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSampleSort, TestSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::sample_sort(parlay::make_slice(s), std::greater<long long>());
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestSampleSort, TestStableSort) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::internal::sample_sort(parlay::make_slice(s), std::less<UnstablePair>(), true);
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestSampleSort, TestStableSortCustomCompare) {
  auto s = parlay::tabulate(100000, [](int i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = i;
    return x;
  });
  auto sorted = parlay::internal::sample_sort(parlay::make_slice(s), std::greater<UnstablePair>(), true);
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::rbegin(s), std::rend(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::rbegin(sorted), std::rend(sorted)));
}

TEST(TestSampleSort, TestSortInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::sample_sort_inplace(parlay::make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestSampleSort, TestSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::sample_sort_inplace(parlay::make_slice(s), std::greater<long long>());
  std::sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestSampleSort, TestSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::sample_sort_inplace(parlay::make_slice(s), std::less<UncopyableThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

namespace parlay {
  // Specialize std::unique_ptr to be considered trivially relocatable
  template<typename T>
  struct is_trivially_relocatable<std::unique_ptr<T>> : public std::true_type { };
}

TEST(TestSampleSort, TestSortInplaceUniquePtr) {
  auto s = parlay::tabulate(100000, [](long long int i) {
    return std::make_unique<long long int>((50021 * i + 61) % (1 << 20));
  });
  auto s2 = parlay::tabulate(100000, [](long long int i) {
    return std::make_unique<long long int>((50021 * i + 61) % (1 << 20));
  });
  parlay::internal::sample_sort_inplace(parlay::make_slice(s), [](const auto& a, const auto& b) { return *a < *b; });
  std::stable_sort(std::begin(s2), std::end(s2), [](const auto& a, const auto& b) { return *a < *b; });
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s), [](const auto& a, const auto& b) { return *a < *b; }));
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(*s[i], *s2[i]);
  }
}

TEST(TestSampleSort, TestSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::sample_sort_inplace(parlay::make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}
