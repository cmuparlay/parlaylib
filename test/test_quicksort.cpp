#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/slice.h>

#include <parlay/internal/quicksort.h>

#include "sorting_utils.h"


TEST(TestQuicksort, TestSortInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::quicksort(make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}


TEST(TestQuicksort, TestSortInplaceCustomCompare) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::quicksort(make_slice(s), std::greater<long long>());
  std::sort(std::rbegin(s2), std::rend(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::rbegin(s), std::rend(s)));
}

TEST(TestQuicksort, TestQuicksortUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::quicksort(make_slice(s), std::less<UncopyableThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestQuicksort, TestQuicksortUniquePtr) {
  auto s = parlay::tabulate(100000, [](long long int i) {
    return std::make_unique<long long int>((50021 * i + 61) % (1 << 20));
  });
  auto s2 = parlay::tabulate(100000, [](long long int i) {
    return std::make_unique<long long int>((50021 * i + 61) % (1 << 20));
  });
  auto cmp = [](const auto& a, const auto& b) { return *a < *b; };
  parlay::internal::quicksort(make_slice(s), cmp);
  std::stable_sort(std::begin(s2), std::end(s2), cmp);
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(*s[i], *s2[i]);
  }
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s), cmp));
}

TEST(TestQuicksort, TestQuicksortSelfReferential) {
  auto s = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::quicksort(make_slice(s), std::less<SelfReferentialThing>());
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestQuicksort, TestSortNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::quicksort(parlay::make_slice(s), std::less<long long>());
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

