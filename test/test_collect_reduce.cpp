#include "gtest/gtest.h"

#include <parlay/primitives.h>
#include <parlay/internal/collect_reduce.h>

#include "sorting_utils.h"

// Sum all of the even and odd numbers in a psuedorandom sequence
// using reduce by key, where the key is the parity
TEST(TestCollectReduce, TestReduceByKey) {
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

TEST(TestCollectReduce, TestGroupByKey) {

}

TEST(TestCollectReduce, TestCountByKey) {

}

TEST(TestCollectReduce, TestRemoveDuplicates) {

}

TEST(TestCollectReduce, TestReduceByIndex) {

}

TEST(TestCollectReduce, TestHistogram) {

}

TEST(TestCollectReduce, TestRemoveDuplicatesByIndex) {

}

TEST(TestCollectReduce, TestGroupByIndex) {

}

