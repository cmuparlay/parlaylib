#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <random>
#include <string>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

TEST(TestPrimitives, TestTabulate) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  ASSERT_EQ(s.size(), 100000);
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(s[i], (50021 * i + 61) % (1 << 20));
  }
}

TEST(TestPrimitives, TestDelayedTabulate) {
  auto s = parlay::delayed_tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  ASSERT_EQ(s.size(), 100000);
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(s[i], (50021 * i + 61) % (1 << 20));
  }
}

TEST(TestPrimitives, TestMap) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto m = parlay::map(s, [](long long x) { return 3*x - 1; });
  ASSERT_EQ(m.size(), s.size());
  for (size_t i = 0; i < 10; i++) {
    ASSERT_EQ(m[i], 3*s[i] - 1);
  }
}

TEST(TestPrimitives, TestDelayedMap) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto m = parlay::delayed_map(s, [](long long x) { return 3*x - 1; });
  ASSERT_EQ(m.size(), s.size());
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(m[i], 3*s[i] - 1);
  }
}

TEST(TestPrimitives, TestCopy) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = parlay::sequence<long long>(100000);
  parlay::copy(s, s2);
  ASSERT_EQ(s, s2);
}

TEST(TestPrimitives, TestReduce) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sum = parlay::reduce(s);
  ASSERT_EQ(sum, std::accumulate(std::begin(s), std::end(s), 0LL));
}

TEST(TestPrimitives, TestReduceMax) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto maxval = parlay::reduce(s, parlay::maxm<long long>());
  ASSERT_EQ(maxval, *std::max_element(std::begin(s), std::end(s)));
}

TEST(TestPrimitives, TestScan) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto [scanz, total] = parlay::scan(s);
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s)-1, std::begin(psums)+1);
  ASSERT_EQ(scanz, psums);
  ASSERT_EQ(total, std::accumulate(std::begin(s), std::end(s), 0LL));
}

TEST(TestPrimitives, TestScanInclusive) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto scanz = parlay::scan_inclusive(s);
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s), std::begin(psums));
  ASSERT_EQ(scanz, psums);
}

TEST(TestPrimitives, TestScanInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sum = std::accumulate(std::begin(s), std::end(s), 0LL);
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s)-1, std::begin(psums)+1);
  auto total = parlay::scan_inplace(s);
  ASSERT_EQ(s, psums);
  ASSERT_EQ(total, sum);
}

TEST(TestPrimitives, TestScanInclusiveInplace) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sum = std::accumulate(std::begin(s), std::end(s), 0LL);
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s), std::begin(psums));
  auto total = parlay::scan_inclusive_inplace(s);
  ASSERT_EQ(s, psums);
  ASSERT_EQ(total, sum);
}

template<typename T>
struct TakeMax {
  T operator()(const T& a, const T& b) { return (std::max)(a, b); }
};

TEST(TestPrimitives, TestScanMax) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto [scanz, total] = parlay::scan(s, parlay::maxm<long long>());
  auto psums = parlay::sequence<long long>(100000);
  psums[0] = parlay::maxm<long long>().identity;
  std::partial_sum(std::begin(s), std::end(s)-1, std::begin(psums)+1, TakeMax<long long>());
  ASSERT_EQ(scanz, psums);
  ASSERT_EQ(total, std::accumulate(std::begin(s), std::end(s), 0LL, TakeMax<long long>()));
}

TEST(TestPrimitives, TestScanInclusiveMax) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto scanz = parlay::scan_inclusive(s, parlay::maxm<long long>());
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s), std::begin(psums), TakeMax<long long>());
  ASSERT_EQ(scanz, psums);
}

TEST(TestPrimitives, TestScanInplaceMax) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sum = std::accumulate(std::begin(s), std::end(s), 0LL, TakeMax<long long>());
  auto psums = parlay::sequence<long long>(100000);
  psums[0] = parlay::maxm<long long>().identity;
  std::partial_sum(std::begin(s), std::end(s)-1, std::begin(psums)+1, TakeMax<long long>());
  auto total = parlay::scan_inplace(s, parlay::maxm<long long>());
  ASSERT_EQ(s, psums);
  ASSERT_EQ(total, sum);
}

TEST(TestPrimitives, TestScanInclusiveInplaceMax) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sum = std::accumulate(std::begin(s), std::end(s), 0LL, TakeMax<long long>());
  auto psums = parlay::sequence<long long>(100000);
  std::partial_sum(std::begin(s), std::end(s), std::begin(psums), TakeMax<long long>());
  auto total = parlay::scan_inclusive_inplace(s, parlay::maxm<long long>());
  ASSERT_EQ(s, psums);
  ASSERT_EQ(total, sum);
}

TEST(TestPrimitives, TestPack) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto b = parlay::tabulate(100000, [](int i) -> bool { return i % 2 == 0; });
  auto packed = parlay::pack(s, b);
  ASSERT_EQ(packed.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(packed[i], 2*i);
  }
}

TEST(TestPrimitives, TestPackConvertible) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto b = parlay::tabulate(100000, [](int i) -> int { return i % 2 == 0; });
  auto packed = parlay::pack(s, b);
  ASSERT_EQ(packed.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(packed[i], 2*i);
  }
}

TEST(TestPrimitives, TestPackInto) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto d = parlay::sequence<int>::uninitialized(50000);
  auto b = parlay::tabulate(100000, [](int i) -> bool { return i % 2 == 0; });
  auto packed = parlay::pack_into_uninitialized(s, b, d);
  ASSERT_EQ(packed, 50000);
  ASSERT_EQ(d.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(d[i], 2*i);
  }
}

TEST(TestPrimitives, TestPackIntoConvertible) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto d = parlay::sequence<int>::uninitialized(50000);
  auto b = parlay::tabulate(100000, [](int i) -> int { return i % 2 == 0; });
  auto packed = parlay::pack_into_uninitialized(s, b, d);
  ASSERT_EQ(packed, 50000);
  ASSERT_EQ(d.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(d[i], 2*i);
  }
}

TEST(TestPrimitives, TestPackIndex) {
  auto s = parlay::tabulate(100000, [](int i) -> int { return i % 2 == 0; });
  auto packed = parlay::pack_index(s);
  ASSERT_EQ(packed.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(packed[i], 2*i);
  }
}

TEST(TestPrimitives, TestPackIndexType) {
  auto s = parlay::tabulate(100000, [](int i) -> int { return i % 2 == 0; });
  auto packed = parlay::pack_index<int>(s);
  ASSERT_EQ(packed.size(), 50000);
  for (size_t i = 0; i < 50000; i++) {
    ASSERT_EQ(packed[i], 2*i);
  }
}

TEST(TestPrimitives, TestFilter) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto f = parlay::filter(s, [](auto x) { return x % 3 == 0; });
  ASSERT_EQ(f.size(), 33334);
  for (size_t i = 0; i < 33334; i++) {
    ASSERT_EQ(f[i], 3*i);
  }
}

TEST(TestPrimitives, TestFilterInto) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto d = parlay::sequence<int>::uninitialized(33334);
  auto f = parlay::filter_into_uninitialized(s, d, [](auto x) { return x % 3 == 0; });
  ASSERT_EQ(d.size(), 33334);
  ASSERT_EQ(f, 33334);
  for (size_t i = 0; i < 33334; i++) {
    ASSERT_EQ(d[i], 3*i);
  }
}

TEST(TestPrimitives, TestMerge) {
  auto s1 = parlay::tabulate(50000, [](int i) { return 2*i; });
  auto s2 = parlay::tabulate(50000, [](int i) { return 2*i + 1; });
  auto s = parlay::merge(s1, s2);
  ASSERT_EQ(s.size(), s1.size() + s2.size());
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], i);
  }
}

TEST(TestPrimitives, TestMergeCustomPredicate) {
  auto s1 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i; }));
  auto s2 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i + 1; }));
  auto s = parlay::merge(s2, s1, std::greater<>());
  ASSERT_EQ(s.size(), s1.size() + s2.size());
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], s.size() - i - 1);
  }
}

TEST(TestPrimitives, TestForEach) {
  parlay::sequence<int> a(100000);
  parlay::for_each(parlay::iota(100000), [&](auto&& i) {
    a[i] = i;
  });
  for (size_t i = 0; i < a.size(); i++) {
    ASSERT_EQ(a[i], i);
  }
}

TEST(TestPrimitives, TestCountIf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto res = parlay::count_if(s, [](int i) { return i % 2 == 0; });
  ASSERT_EQ(res, s.size()/2);
}

TEST(TestPrimitives, TestAllOf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto s1 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i; }));
  auto s2 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i + 1; }));

  ASSERT_FALSE(parlay::all_of(s, [](int x) { return x % 2 == 0; }));
  ASSERT_TRUE(parlay::all_of(s1, [](int x) { return x % 2 == 0; }));
  ASSERT_FALSE(parlay::all_of(s2, [](int x) { return x % 2 == 0; }));
}

TEST(TestPrimitives, TestAnyOf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto s1 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i; }));
  auto s2 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i + 1; }));

  ASSERT_TRUE(parlay::any_of(s, [](int x) { return x % 2 == 0; }));
  ASSERT_TRUE(parlay::any_of(s1, [](int x) { return x % 2 == 0; }));
  ASSERT_FALSE(parlay::any_of(s2, [](int x) { return x % 2 == 0; }));
}

TEST(TestPrimitives, TestNoneOf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto s1 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i; }));
  auto s2 = parlay::reverse(parlay::tabulate(50000, [](int i) { return 2*i + 1; }));

  ASSERT_FALSE(parlay::none_of(s, [](int x) { return x % 2 == 0; }));
  ASSERT_FALSE(parlay::none_of(s1, [](int x) { return x % 2 == 0; }));
  ASSERT_TRUE(parlay::none_of(s2, [](int x) { return x % 2 == 0; }));
}

TEST(TestPrimitives, TestFindIf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto it = parlay::find_if(s, [](int x) { return x >= 61234; });
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 61234);
  auto it2 = parlay::find_if(s, [](int x) { return x >= 1000000; });
  ASSERT_EQ(it2, s.end());
}

TEST(TestPrimitives, TestFind) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto it = parlay::find(s, 61234);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 61234);
  auto it2 = parlay::find(s, 1000000);
  ASSERT_EQ(it2, s.end());
}

TEST(TestPrimitives, TestFindIfNot) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto it = parlay::find_if_not(s, [](int x) { return x < 61234; });
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 61234);
  auto it2 = parlay::find_if_not(s, [](int x) { return x < 1000000; });
  ASSERT_EQ(it2, s.end());
}

TEST(TestPrimitives, TestFindFirstOf) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto p = parlay::sequence<int>{1000000, 61234, 1000, 23451, 76473};
  auto it = parlay::find_first_of(s, p);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 1000);
  auto p2 = parlay::tabulate(100, [](int i) { return 1000000 + i; });
  auto it2 = parlay::find_first_of(s, p2);
  ASSERT_EQ(it2, s.end());
}

TEST(TestPrimitives, TestFindEnd) {
  auto s = parlay::tabulate(100000, [](int i) { return i % 100; });
  auto p = parlay::tabulate(100, [](int i) { return i; });
  auto it = parlay::find_end(s, p);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(it, s.end() - 100);
  ASSERT_EQ(*it, 0);

  auto p2 = parlay::tabulate(100, [](int i) { return i+1; });
  auto it2 = parlay::find_end(s, p2);
  ASSERT_EQ(it2, s.end());

  auto p3 = parlay::sequence<int>{};
  auto it3 = parlay::find_end(s, p3);
  ASSERT_EQ(it3, s.end());
}

TEST(TestPrimitives, TestAdjacentFind) {
  auto s = parlay::tabulate(100000, [](int i) { return ((i/1000) % 2) ? (999 - i % 1000) : (i % 1000); });
  auto it = parlay::adjacent_find(s);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(it, s.begin() + 999);

  auto s2 = parlay::tabulate(100000, [](int i) { return i % 100; });
  auto it2 = parlay::adjacent_find(s2);
  ASSERT_EQ(it2, s2.end());
}

TEST(TestPrimitives, TestMismatch) {
  auto s1 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto s2 = parlay::tabulate(100000, [](int i) { return i % 10001; });
  auto [it1, it2] = parlay::mismatch(s1, s2);
  ASSERT_NE(it1, s1.end());
  ASSERT_NE(it2, s2.end());
  ASSERT_EQ(it1, s1.begin() + 10000);
  ASSERT_EQ(it2, s2.begin() + 10000);

  auto s3 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto [it3, it4] = parlay::mismatch(s1, s3);
  ASSERT_EQ(it3, s1.end());
  ASSERT_EQ(it4, s3.end());

  auto s4 = parlay::tabulate(50000, [](int i) { return i % 10000; });
  auto [it5, it6] = parlay::mismatch(s1, s4);
  ASSERT_EQ(it5, s1.begin() + 50000);
  ASSERT_EQ(it6, s4.end());
}

TEST(TestPrimitives, TestSearch) {
  auto s = parlay::tabulate(100000, [](int i) { return i == 0 ? -1 : (i % 100); });
  auto p = parlay::tabulate(100, [](int i) { return i; });
  auto it = parlay::search(s, p);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(it, s.begin() + 100);
  ASSERT_EQ(*it, 0);

  auto p2 = parlay::tabulate(100, [](int i) { return i+1; });
  auto it2 = parlay::search(s, p2);
  ASSERT_EQ(it2, s.end());

  auto p3 = parlay::sequence<int>{};
  auto it3 = parlay::search(s, p3);
  ASSERT_EQ(it3, s.begin());
}

TEST(TestPrimitives, TestEqual) {
  auto s1 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto s2 = parlay::tabulate(100000, [](int i) { return i % 10001; });
  auto s3 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto s4 = parlay::tabulate(50000, [](int i) { return i % 10000; });

  ASSERT_FALSE(parlay::equal(s1, s2));
  ASSERT_TRUE(parlay::equal(s1, s3));
  ASSERT_FALSE(parlay::equal(s1, s4));
}

TEST(TestPrimitives, TestLexicographicalCompare) {
  auto s1 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto s2 = parlay::tabulate(100000, [](int i) { return i % 10001; });
  auto s3 = parlay::tabulate(100000, [](int i) { return i % 10000; });
  auto s4 = parlay::tabulate(50000, [](int i) { return i % 10000; });
  auto s5 = s4;
  s5.back() += 1;

  ASSERT_TRUE(parlay::lexicographical_compare(s1, s2));
  ASSERT_FALSE(parlay::lexicographical_compare(s2, s1));
  ASSERT_FALSE(parlay::lexicographical_compare(s1, s3));
  ASSERT_TRUE(parlay::lexicographical_compare(s4, s1));
  ASSERT_TRUE(parlay::lexicographical_compare(s1, s5));
}

TEST(TestPrimitives, TestUnique) {
  auto s = parlay::tabulate(100000, [](int i) { return i / 2; });
  auto ans = parlay::tabulate(50000, [](int i) { return i; });
  auto u = parlay::unique(s);
  ASSERT_EQ(u.size(), 50000);
  ASSERT_EQ(u, ans);
}

TEST(TestPrimitives, TestMinElement) {
  auto s = parlay::tabulate(100000, [](int i) { return (i+42424) % 100000; });
  auto it = parlay::min_element(s);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 0);

  auto s2 = parlay::sequence<int>{};
  auto it2 = parlay::min_element(s2);
  ASSERT_EQ(it2, s2.begin());
}

TEST(TestPrimitives, TestMaxElement) {
  auto s = parlay::tabulate(100000, [](int i) { return (i+67890) % 100000; });
  auto it = parlay::max_element(s);
  ASSERT_NE(it, s.end());
  ASSERT_EQ(*it, 99999);

  auto s2 = parlay::sequence<int>{};
  auto it2 = parlay::max_element(s2);
  ASSERT_EQ(it2, s2.begin());
}

TEST(TestPrimitives, TestMinMaxElement) {
  auto s = parlay::tabulate(100000, [](int i) { return (i+67890) % 100000; });
  auto [min_it, max_it] = parlay::minmax_element(s);
  ASSERT_NE(min_it, s.end());
  ASSERT_NE(max_it, s.end());
  ASSERT_EQ(*min_it, 0);
  ASSERT_EQ(*max_it, 99999);

  auto s2 = parlay::sequence<int>{};
  auto [it1, it2] = parlay::minmax_element(s2);
  ASSERT_EQ(it1, s2.begin());
  ASSERT_EQ(it2, s2.begin());
}

TEST(TestPrimitives, TestReverse) {
  auto s = parlay::tabulate(100000, [](int i) { return i % 100000; });
  auto r = parlay::reverse(s);
  ASSERT_EQ(s.size(), r.size());
  for (size_t i = 0; i < r.size(); i++) {
    ASSERT_EQ(r[i], s[s.size()-1-i]);
  }
}

TEST(TestPrimitives, TestReverseInplace) {
  auto s = parlay::tabulate(100000, [](int i) { return i % 100000; });
  parlay::reverse_inplace(s);
  ASSERT_EQ(s.size(), 100000);
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], 99999-i);
  }
}

TEST(TestPrimitives, TestRotate) {
  auto s = parlay::tabulate(100000, [](int i) { return i % 100000; });
  auto answer = parlay::tabulate(100000, [](int i) { return (i + 42000) % 100000; });
  auto r = parlay::rotate(s, 42000);
  ASSERT_EQ(r.size(), s.size());
  ASSERT_EQ(r, answer);
}

TEST(TestPrimitives, TestIsSorted) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto s2 = parlay::tabulate(100000, [](int i) { return (i+67890) % 100000; });
  auto s3 = parlay::sequence<int>{};

  ASSERT_TRUE(parlay::is_sorted(s));
  ASSERT_FALSE(parlay::is_sorted(s2));
  ASSERT_TRUE(parlay::is_sorted(s3));
}

TEST(TestPrimitives, TestIsSortedUntil) {
  auto s = parlay::tabulate(100000, [](int i) { return i; });
  auto s2 = parlay::tabulate(100000, [](int i) { return (i+67890) % 100000; });
  auto s3 = parlay::sequence<int>{};

  auto it1 = parlay::is_sorted_until(s);
  auto it2 = parlay::is_sorted_until(s2);
  auto it3 = parlay::is_sorted_until(s3);

  ASSERT_EQ(it1, std::end(s));
  ASSERT_EQ(it2, std::end(s2) - 67890);
  ASSERT_EQ(it3, std::begin(s3));
}

TEST(TestPrimitives, TestIsPartitioned) {
  auto s = parlay::tabulate(100000,  [](int i) { return i; });
  auto s2 = parlay::tabulate(100000, [](int i) { return (i+67890) % 100000; });
  auto s3 = parlay::sequence<int>{};

  ASSERT_TRUE(parlay::is_partitioned(s, [](int x) { return x <= 50000; }));
  ASSERT_FALSE(parlay::is_partitioned(s2, [](int x) { return x <= 50000; }));
  ASSERT_TRUE(parlay::is_partitioned(s3, [](int x) { return x <= 50000; }));
}

TEST(TestPrimitives, TestRemove) {
  auto s = parlay::tabulate(100000,  [](int i) { return i % 10; });
  auto r = parlay::remove(s, 5);
  ASSERT_EQ(r.size(), 90000);
  for (size_t i = 0; i < r.size(); i++) {
    ASSERT_NE(r[i], 5);
  }
}

TEST(TestPrimitives, TestHistogramByIndex) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto hist = parlay::histogram_by_index(s, 1 << 20);
  ASSERT_EQ(hist.size(), 1 << 20);
  auto cnts = parlay::sequence<size_t>(1 << 20, 0);
  for (auto x : s) {
    cnts[static_cast<size_t>(x)]++;
  }
  ASSERT_TRUE(std::equal(hist.begin(), hist.end(), cnts.begin()));
}

TEST(TestPrimitives, TestFlatten) {
  auto seqs = parlay::tabulate(100, [](size_t i) {
    return parlay::tabulate(1000, [i](size_t j) {
      return 1000 * i + j;
    });
  });
  auto seq = parlay::flatten(seqs);
  auto answer = parlay::tabulate(100000, [](size_t i) {
    return i;
  });
  ASSERT_EQ(seq.size(), 100000);
  ASSERT_EQ(seq, answer);
}

TEST(TestPrimitives, TestFlattenRvalueRef) {
  auto seqs = parlay::tabulate(100, [](size_t i) {
    return parlay::tabulate(1000, [i](size_t j) {
      return 1000 * i + j;
    });
  });
  auto seq = parlay::flatten(std::move(seqs));
  auto answer = parlay::tabulate(100000, [](size_t i) {
    return i;
  });
  ASSERT_EQ(seq.size(), 100000);
  ASSERT_EQ(seq, answer);
  ASSERT_TRUE(seqs.empty());
}

TEST(TestPrimitives, TestFlattenNestedDelayed) {
  auto G = parlay::tabulate(10000, [](size_t i) {
    if (parlay::hash64_2(i) % 2 != 0)
      return parlay::tabulate(i, [](size_t j) -> int {
        return j;
      });
    else
      return parlay::sequence<int>{};
  });
  
  auto seq = parlay::flatten(parlay::delayed_tabulate(G.size(), [&](int i) {
    return parlay::delayed_map(G[i], [=](int x) {
      return std::make_pair(x, i);
    });
  }));

  auto seq2 = parlay::flatten(parlay::tabulate(G.size(), [&](int i) {
    return parlay::map(G[i], [=](int x) {
      return std::make_pair(x, i);
    });
  }));

  ASSERT_EQ(seq, seq2);
}

TEST(TestPrimitives, TestFlattenDelayed) {
  auto G = parlay::tabulate(10000, [](size_t i) {
    if (parlay::hash64_2(i) % 2 != 0)
      return parlay::tabulate(i, [](size_t j) -> int {
        return j;
      });
    else
      return parlay::sequence<int>{};
  });

  // Use parlay::sequence with std::allocator as the inner sequence to better catch use-after-free
  auto seq = parlay::flatten(parlay::delayed_tabulate(G.size(), [&](int i) {
    return parlay::to_sequence<std::pair<int,int>, std::allocator<int>>(parlay::map(G[i], [=](int x) {
      return std::make_pair(x, i);
    }));
  }));

  auto seq2 = parlay::flatten(parlay::tabulate(G.size(), [&](int i) {
    return parlay::map(G[i], [=](int x) {
      return std::make_pair(x, i);
    });
  }));

  ASSERT_EQ(seq, seq2);
}

TEST(TestPrimitives, TestTokens) {
  auto chars = parlay::to_sequence(std::string(" The quick\tbrown fox jumped over  the lazy\ndog "));
  auto words = parlay::sequence<parlay::sequence<char>> {
    parlay::to_sequence(std::string("The")),
    parlay::to_sequence(std::string("quick")),
    parlay::to_sequence(std::string("brown")),
    parlay::to_sequence(std::string("fox")),
    parlay::to_sequence(std::string("jumped")),
    parlay::to_sequence(std::string("over")),
    parlay::to_sequence(std::string("the")),
    parlay::to_sequence(std::string("lazy")),
    parlay::to_sequence(std::string("dog"))
  };
  auto tokens = parlay::tokens(chars);
  ASSERT_EQ(words, tokens);
}

TEST(TestPrimitives, TestMapTokens) {
  auto sentence = parlay::to_sequence(std::string(" The quick\tbrown fox jumped over  the lazy\ndog "));
  auto words = parlay::sequence<parlay::sequence<char>> {
    parlay::to_sequence(std::string("The")),
    parlay::to_sequence(std::string("quick")),
    parlay::to_sequence(std::string("brown")),
    parlay::to_sequence(std::string("fox")),
    parlay::to_sequence(std::string("jumped")),
    parlay::to_sequence(std::string("over")),
    parlay::to_sequence(std::string("the")),
    parlay::to_sequence(std::string("lazy")),
    parlay::to_sequence(std::string("dog"))
  };
  auto lengths = parlay::map_tokens(sentence, [](auto&& token) { return token.size(); });
  auto real_lengths = parlay::map(words, [](auto&& word) { return word.size(); });
  
  ASSERT_EQ(lengths, real_lengths);
}

TEST(TestPrimitives, TestMapTokensVoid) {
  auto chars = parlay::to_sequence(std::string(" The quick\tbrown fox jumped over  the lazy\ndog "));
  auto words = parlay::sequence<parlay::sequence<char>> {
    parlay::to_sequence(std::string("The")),
    parlay::to_sequence(std::string("quick")),
    parlay::to_sequence(std::string("brown")),
    parlay::to_sequence(std::string("fox")),
    parlay::to_sequence(std::string("jumped")),
    parlay::to_sequence(std::string("over")),
    parlay::to_sequence(std::string("the")),
    parlay::to_sequence(std::string("lazy")),
    parlay::to_sequence(std::string("dog"))
  };
  
  std::array<std::atomic<size_t>, 10> lengths;
  for (size_t l = 0; l < 10; l++) {
    lengths[l].store(0);
  }
  
  parlay::map_tokens(chars, [&lengths](auto&& token) -> void {
    lengths[token.size()].fetch_add(1);
  });
  
  auto real_lengths = parlay::map(words, [](auto&& word) { return word.size(); });
  
  for (size_t l = 0; l < 10; l++) {
    ASSERT_EQ(lengths[l], parlay::count(real_lengths, l));
  }
}

TEST(TestPrimitives, TestSplitAt) {
  auto seq = parlay::sequence<int>(999999, 1);
  auto seqs = parlay::split_at(seq, parlay::delayed_tabulate(999999,[&](size_t i) -> bool {
    return i % 1000 == 999;
  }));
  
  auto ans = parlay::tabulate(1000, [](int i) -> parlay::sequence<int> {
      return parlay::sequence((i==999) ? 999 : 1000, 1);});
  
  ASSERT_EQ(seqs, ans);
}

TEST(TestPrimitives, TestMapSplitAt) {
  auto seq = parlay::sequence<int>(999999, 1);
  auto map_reduces = parlay::map_split_at(seq,
    parlay::delayed_tabulate(999999, [&](size_t i) -> bool { return i % 1000 == 999; }),
    [](const auto& s) { return parlay::reduce(s); });
  
  auto answer = parlay::tabulate(1000, [](int i) -> int {
      return (i == 999) ? 999 : 1000;});
  
  ASSERT_EQ(map_reduces, answer);
}

TEST(TestPrimitives, TestRemoveDuplicatesOrdered) {
  auto s = parlay::tabulate(100000,  [](int i) { return i % 1000; });
  auto r = parlay::remove_duplicates_ordered(s);
  ASSERT_EQ(r.size(), 1000);
  for (size_t i = 0; i < r.size(); i++) {
    ASSERT_EQ(r[i], i);
  }
}

TEST(TestPrimitives, TestAppend) {
  auto s1 = parlay::tabulate(100000,  [](int i) { return i % 1000; });
  auto s2 = parlay::tabulate(100000,  [](int i) { return (i % 1000) + 1000; });
  auto answer = parlay::tabulate(200000,  [](int i) { return (i < 100000) ? (i % 1000) : ((i % 1000) + 1000); });
  auto res = parlay::append(s1, s2);
  ASSERT_EQ(res, answer);
}

TEST(TestPrimitives, TestMapMaybe) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::map_maybe(seq, [](auto x) -> std::optional<int> {
    if (x % 2 == 0) return std::make_optional(x); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));
}

TEST(TestPrimitives, TestZip) {
  auto a = parlay::tabulate(50000, [](size_t i) { return i+1; });
  auto b = parlay::tabulate(50000, [](size_t i) { return i+2; });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x+1);
  }
}

TEST(TestPrimitives, TestRank) {
  std::default_random_engine eng{2022};

  {
    // Rank of iota should be itself
    auto s = parlay::to_sequence(parlay::iota<size_t>(100000));
    auto sr = parlay::rank(s);
    ASSERT_EQ(s, sr);
  }

  {
    // Rank of a shuffled iota should be itself
    auto s = parlay::to_sequence(parlay::iota<size_t>(100000));
    std::shuffle(s.begin(), s.end(), eng);
    auto sr = parlay::rank(s);
    ASSERT_EQ(s, sr);
  }

  {
    auto s = parlay::tabulate(100000, [](size_t i) { return std::make_pair(std::to_string(i), i); });
    std::sort(s.begin(), s.end());
    for (size_t i = 0; i < s.size(); i++) {
      s[i].second = i;
    }
    std::shuffle(s.begin(), s.end(), eng);
    auto sr = parlay::rank(s);
    for (size_t i = 0; i < s.size(); i++) {
      ASSERT_EQ(sr[i], s[i].second);
    }
  }

  {
    // Check for stability
    parlay::sequence<int> s = {0, 1, 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9};
    parlay::sequence<size_t> ranks = {0, 2, 1, 3, 4, 6, 5, 7, 8, 10, 9, 11, 12, 14, 13, 15, 16, 18, 17, 19};
    auto sr = parlay::rank(s);
    ASSERT_EQ(sr, ranks);
  }
}

TEST(TestPrimitives, TestKthSmallestCopy) {
  std::default_random_engine eng{2022};
  auto s = parlay::to_sequence(parlay::iota<size_t>(100000));
  std::shuffle(s.begin(), s.end(), eng);

  ASSERT_EQ(parlay::kth_smallest_copy(s, 0), 0);
  ASSERT_EQ(parlay::kth_smallest_copy(s, 50000), 50000);
  ASSERT_EQ(parlay::kth_smallest_copy(s, 99999), 99999);

  for (size_t i = 7919; i < 100000; i+= 7907) {
    ASSERT_EQ(parlay::kth_smallest_copy(s, i), i);
  }
}

TEST(TestPrimitives, TestKthSmallest) {
  std::default_random_engine eng{2022};
  auto s = parlay::to_sequence(parlay::iota<size_t>(100000));
  std::shuffle(s.begin(), s.end(), eng);

  ASSERT_EQ(*parlay::kth_smallest(s, 0), 0);
  ASSERT_EQ(*parlay::kth_smallest(s, 50000), 50000);
  ASSERT_EQ(*parlay::kth_smallest(s, 99999), 99999);

  for (size_t i = 7919; i < 100000; i+= 7907) {
    ASSERT_EQ(*parlay::kth_smallest(s, i), i);
  }
}
