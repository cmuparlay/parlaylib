#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/type_traits.h>
#include <parlay/utilities.h>

#include <parlay/internal/counting_sort.h>

#include "sorting_utils.h"

constexpr size_t num_buckets = 1 << 16;

TEST(TestCountingSort, TestCountingSort) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % num_buckets;
  });
  auto [sorted, offsets] = parlay::internal::count_sort(parlay::make_slice(s), [](auto x) { return x; }, num_buckets);
  ASSERT_EQ(s.size(), sorted.size());
  std::sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestCountingSort, TestCountingSortUnstable) {
  auto s = parlay::tabulate(100000, [](long long i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % num_buckets;
    x.y = 0;
    return x;
  });
  auto [sorted, offsets] = parlay::internal::count_sort(parlay::make_slice(s), [](const auto& x) -> unsigned long long {
    return x.x;
  }, num_buckets);
  ASSERT_EQ(s.size(), sorted.size());
  std::stable_sort(std::begin(s), std::end(s));
  ASSERT_EQ(s, sorted);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestCountingSort, TestCountingSortInplaceCustomKey) {
  auto s = parlay::tabulate(100000, [](long long i) -> UnstablePair {
    UnstablePair x;
    x.x = (53 * i + 61) % (1 << 10);
    x.y = 0;
    return x;
  });
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), [](const auto& x) -> unsigned long long {
    return x.x;
  }, num_buckets);
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestCountingSort, TestCountingSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing((100000-i) % num_buckets);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing((100000-i) % num_buckets);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), [](const auto& a) { return a.x; }, num_buckets);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s))); 
}

TEST(TestCountingSort, TestCountingSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % num_buckets;
  });
  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), [](auto x) { return x; }, num_buckets);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2); 
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

namespace parlay {
  // Specialize std::unique_ptr to be considered trivially relocatable
  template<typename T>
  struct is_trivially_relocatable<std::unique_ptr<T>> : public std::true_type { };
}

TEST(TestCountingSort, TestCountingSortInplaceUniquePtr) {
  auto s = parlay::tabulate(100000, [](size_t i) {
    return std::make_unique<int>((50021 * i + 61) % num_buckets);
  });
  auto sorted = parlay::tabulate(100000, [](size_t i) {
    return std::make_unique<int>((50021 * i + 61) % num_buckets);
  });
  std::sort(std::begin(sorted), std::end(sorted), [](const auto& p1, const auto& p2) {
    return *p1 < *p2;
  });
  parlay::internal::count_sort_inplace(parlay::make_slice(s), [](const auto& p) { return *p; }, num_buckets);
  ASSERT_EQ(s.size(), sorted.size());
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(*s[i], *sorted[i]);
  }
}

TEST(TestCountingSort, TestCountingSortInplaceSelfReferential) {
  auto s = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i % num_buckets);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i % num_buckets);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), [](const auto& p) { return p.x; }, num_buckets);
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}
