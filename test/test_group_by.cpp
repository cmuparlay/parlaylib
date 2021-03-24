#include "gtest/gtest.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <parlay/internal/group_by.h>

TEST(TestGroupBy, TestGroupByKeySorted) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_key_sorted(a);

  ASSERT_EQ(grouped.size(), 3);
  ASSERT_EQ(grouped[0].first, 1);
  ASSERT_EQ(grouped[1].first, 2);
  ASSERT_EQ(grouped[2].first, 3);
  ASSERT_EQ(grouped[0].second.size(), 3);
  ASSERT_EQ(grouped[1].second.size(), 5);
  ASSERT_EQ(grouped[2].second.size(), 3);
}

TEST(TestGroupBy, TestReduceByKey) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };

  auto reduced = parlay::reduce_by_key(a);

  ASSERT_EQ(reduced.size(), 3);
  std::map<int,int> results;

  for (const auto& result : reduced) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1], 42);
  ASSERT_EQ(results[2], 113);
  ASSERT_EQ(results[3], 105);
}

// Sum all of the even and odd numbers in a psuedorandom sequence
// using reduce by key, where the key is the parity
TEST(TestGroupBy, TestReduceByKeyLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto key_vals = parlay::delayed_map(s, [](auto x) { return std::make_pair(x % 2, x); } );
  auto result = parlay::reduce_by_key(key_vals, parlay::addm<unsigned long long>{});
  unsigned long long odd_sum = 0, even_sum = 0;
  for (auto x : s) {
    if (x % 2 == 0) even_sum += x;
    else odd_sum += x;
  }
  ASSERT_EQ(result.size(), 2);
  ASSERT_TRUE(result[0].first == 0 || result[0].first == 1);
  ASSERT_TRUE(result[1].first == 0 || result[1].first == 1);

  unsigned long long odd_res = 0, even_res = 0;
  if (result[0].first == 0) {
    even_res = result[0].second;
    odd_res = result[1].second;
  }
  else {
    even_res = result[1].second;
    odd_res = result[0].second;
  }
  ASSERT_EQ(even_sum, even_res);
  ASSERT_EQ(odd_sum, odd_res);
}

TEST(TestGroupBy, TestGroupByKey) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_key(a);

  ASSERT_EQ(grouped.size(), 3);
  std::map<int, parlay::sequence<int>> results;

  for (const auto& result : grouped) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1].size(), 3);
  ASSERT_EQ(results[2].size(), 5);
  ASSERT_EQ(results[3].size(), 3);
}

TEST(TestGroupBy, TestCountByKey) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto counts = parlay::count_by_key(a);

  ASSERT_EQ(counts.size(), 3);
  std::map<int, int> results;

  for (const auto& result : counts) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1], 3);
  ASSERT_EQ(results[2], 5);
  ASSERT_EQ(results[3], 3);
}

TEST(TestGroupBy, TestRemoveDuplicates) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto deduped = parlay::remove_duplicates(a);

  ASSERT_EQ(deduped.size(), 3);
  ASSERT_EQ(std::set<int>(deduped.begin(), deduped.end()), (std::set<int>{1,2,3}));
}

TEST(TestGroupBy, TestReduceByIndex) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };

  auto reduced = parlay::reduce_by_index(a, 4);

  ASSERT_EQ(reduced.size(), 4);

  ASSERT_EQ(reduced[0], 0);
  ASSERT_EQ(reduced[1], 42);
  ASSERT_EQ(reduced[2], 113);
  ASSERT_EQ(reduced[3], 105);
}

TEST(TestGroupBy, TestHistogram) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto counts = parlay::histogram(a, 4);

  ASSERT_EQ(counts.size(), 4);

  ASSERT_EQ(counts[0], 0);
  ASSERT_EQ(counts[1], 3);
  ASSERT_EQ(counts[2], 5);
  ASSERT_EQ(counts[3], 3);
}

TEST(TestGroupBy, TestRemoveDuplicatesByIndex) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto deduped = parlay::remove_duplicates_by_index(a, 4);

  ASSERT_EQ(deduped.size(), 3);
  ASSERT_EQ(std::set<int>(deduped.begin(), deduped.end()), (std::set<int>{1,2,3}));
}

TEST(TestGroupBy, TestGroupByIndex) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_index(a, 4);

  ASSERT_EQ(grouped.size(), 4);

  ASSERT_EQ(grouped[0].size(), 0);
  ASSERT_EQ(grouped[1].size(), 3);
  ASSERT_EQ(grouped[2].size(), 5);
  ASSERT_EQ(grouped[3].size(), 3);
}
