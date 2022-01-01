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

constexpr size_t num_buckets = size_t{1} << 16;

TEST(TestCountingSort, TestCountingSort) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % num_buckets;
  });
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return s[i]; });

  auto sorted = parlay::internal::count_sort(parlay::make_slice(s), keys, num_buckets).first;
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
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return s[i].x; });

  auto sorted = parlay::internal::count_sort(parlay::make_slice(s), keys, num_buckets).first;
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
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return s[i].x; });

  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), keys, num_buckets);
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestCountingSort, TestCountingSortInplaceUncopyable) {
  auto s = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing((100000 - i) % num_buckets);
  });
  auto s2 = parlay::tabulate(100000, [](int i) -> UncopyableThing {
    return UncopyableThing((100000 - i) % num_buckets);
  });
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return s[i].x; });

  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), keys, num_buckets);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestCountingSort, TestCountingSortInplaceNonContiguous) {
  auto ss = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % num_buckets;
  });
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return ss[i]; });

  auto s = std::deque<long long>(ss.begin(), ss.end());
  auto s2 = s;
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), keys, num_buckets);
  std::sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

namespace parlay {
// Specialize std::unique_ptr to be considered trivially relocatable
template<typename T>
struct is_trivially_relocatable<std::unique_ptr<T>> : public std::true_type {
};
}

TEST(TestCountingSort, TestCountingSortInplaceUniquePtr) {
  auto s = parlay::tabulate(100000, [](long long i) {
    return std::make_unique<long long>((51 * i + 61) % num_buckets);
  });
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return *s[i]; });

  auto sorted = parlay::tabulate(100000, [](long long i) {
    return std::make_unique<long long>((51 * i + 61) % num_buckets);
  });
  std::sort(std::begin(sorted), std::end(sorted), [](const auto &p1, const auto &p2) {
    return *p1 < *p2;
  });
  parlay::internal::count_sort_inplace(parlay::make_slice(s), keys, num_buckets);
  ASSERT_EQ(s.size(), sorted.size());
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(*s[i], *sorted[i]);
  }
}

TEST(TestCountingSort, TestCountingSortInplaceSelfReferential) {
  auto s = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i % num_buckets);
  });
  auto keys = parlay::delayed_tabulate(100000, [&](size_t i) { return s[i].x; });

  auto s2 = parlay::tabulate(100000, [](int i) -> SelfReferentialThing {
    return SelfReferentialThing(i % num_buckets);
  });
  ASSERT_EQ(s, s2);
  parlay::internal::count_sort_inplace(parlay::make_slice(s), keys, num_buckets);
  std::stable_sort(std::begin(s2), std::end(s2));
  ASSERT_EQ(s, s2);
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}
