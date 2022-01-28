#include "gtest/gtest.h"

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <parlay/delayed_sequence.h>
#include <parlay/range.h>
#include <parlay/sequence.h>
#include <parlay/slice.h>

static_assert(parlay::is_forward_range_v<std::forward_list<int>>);
static_assert(parlay::is_forward_range_v<const std::forward_list<int>>);
static_assert(parlay::is_common_range_v<std::forward_list<int>>);

static_assert(parlay::is_forward_range_v<std::unordered_set<int>>);
static_assert(parlay::is_forward_range_v<const std::unordered_set<int>>);
static_assert(parlay::is_common_range_v<std::unordered_set<int>>);

static_assert(parlay::is_forward_range_v<std::unordered_map<int,int>>);
static_assert(parlay::is_forward_range_v<const std::unordered_map<int,int>>);
static_assert(parlay::is_common_range_v<std::unordered_map<int,int>>);

static_assert(parlay::is_forward_range_v<std::unordered_multiset<int>>);
static_assert(parlay::is_forward_range_v<const std::unordered_multiset<int>>);
static_assert(parlay::is_common_range_v<std::unordered_multiset<int>>);

static_assert(parlay::is_forward_range_v<std::unordered_multimap<int,int>>);
static_assert(parlay::is_forward_range_v<const std::unordered_multimap<int,int>>);
static_assert(parlay::is_common_range_v<std::unordered_multimap<int,int>>);

static_assert(parlay::is_bidirectional_range_v<std::list<int>>);
static_assert(parlay::is_bidirectional_range_v<const std::list<int>>);
static_assert(parlay::is_common_range_v<std::list<int>>);

static_assert(parlay::is_bidirectional_range_v<std::set<int>>);
static_assert(parlay::is_bidirectional_range_v<const std::set<int>>);
static_assert(parlay::is_common_range_v<std::set<int>>);

static_assert(parlay::is_bidirectional_range_v<std::map<int,int>>);
static_assert(parlay::is_bidirectional_range_v<const std::map<int,int>>);
static_assert(parlay::is_common_range_v<std::map<int,int>>);

static_assert(parlay::is_bidirectional_range_v<std::multiset<int>>);
static_assert(parlay::is_bidirectional_range_v<const std::multiset<int>>);
static_assert(parlay::is_common_range_v<std::multiset<int>>);

static_assert(parlay::is_bidirectional_range_v<std::multimap<int,int>>);
static_assert(parlay::is_bidirectional_range_v<const std::multimap<int,int>>);
static_assert(parlay::is_common_range_v<std::multimap<int,int>>);

static_assert(parlay::is_random_access_range_v<std::array<int, 100>>);
static_assert(parlay::is_random_access_range_v<const std::array<int, 100>>);
static_assert(parlay::is_common_range_v<std::array<int, 100>>);

static_assert(parlay::is_random_access_range_v<std::vector<int>>);
static_assert(parlay::is_random_access_range_v<const std::vector<int>>);
static_assert(parlay::is_common_range_v<std::vector<int>>);

static_assert(parlay::is_random_access_range_v<std::deque<int>>);
static_assert(parlay::is_random_access_range_v<const std::deque<int>>);
static_assert(parlay::is_common_range_v<std::deque<int>>);

static_assert(parlay::is_random_access_range_v<parlay::sequence<int>>);
static_assert(parlay::is_random_access_range_v<const parlay::sequence<int>>);
static_assert(parlay::is_contiguous_range_v<parlay::sequence<int>>);
static_assert(parlay::is_contiguous_range_v<const parlay::sequence<int>>);
static_assert(parlay::is_common_range_v<parlay::sequence<int>>);

struct F { auto operator()(size_t) const -> int; };
using delayed_seq_int = decltype(parlay::delayed_seq<int>(10, F{}));
static_assert(parlay::is_random_access_range_v<delayed_seq_int>);
static_assert(parlay::is_random_access_range_v<const delayed_seq_int>);
static_assert(parlay::is_common_range_v<delayed_seq_int>);

using slice_int = parlay::slice<parlay::range_iterator_type_t<parlay::sequence<int>>, parlay::range_sentinel_type_t<parlay::sequence<int>>>;
static_assert(parlay::is_random_access_range_v<slice_int>);
static_assert(parlay::is_random_access_range_v<const slice_int>);
static_assert(parlay::is_common_range_v<slice_int>);

using slice_const_int = parlay::slice<parlay::range_iterator_type_t<const parlay::sequence<int>>, parlay::range_sentinel_type_t<const parlay::sequence<int>>>;
static_assert(parlay::is_random_access_range_v<slice_const_int>);
static_assert(parlay::is_random_access_range_v<const slice_const_int>);
static_assert(parlay::is_common_range_v<slice_const_int>);

TEST(TestRange, TestVector) {
  auto a = std::vector<int>{1,2,3};
  ASSERT_EQ(a.size(), 3);
}

TEST(TestRange, TestArray) {
  auto a = std::array<int, 3>{1,2,3};
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestSequence) {
  auto a = parlay::sequence<int>{1,2,3};
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestDelayedSequence) {
  auto a = parlay::delayed_seq<int>(10, [](int x) { return x; });
  ASSERT_EQ(parlay::size(a), 10);
}

TEST(TestRange, TestSlice) {
  std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a);
  ASSERT_EQ(parlay::size(s), 3);
}
