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

static_assert(parlay::is_bounded_array_v<int[10]>);
static_assert(!parlay::is_bounded_array_v<int[]>);

TEST(TestRange, TestBoundedArray) {
  int a[10];
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 10);
}

TEST(TestRange, TestSizedSentinel) {
  struct my_range {
    my_range() : my_vector(10) {}
    auto begin() { return my_vector.begin(); }
    auto end() { return my_vector.end(); }
    std::vector<int> my_vector;
  };

  my_range a;
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 10);
}

namespace nonmember_sized_range {

struct my_range {
  my_range() = default;
  auto begin() const { return std::forward_list<int>::iterator{}; }
  auto end() const { return std::forward_list<int>::iterator{}; }
  using iterator = std::forward_list<int>::iterator;
};

// Need both the non-const and const versions for this to work.
// Otherwise, ADL prefers the deleted size functions instead
size_t size(my_range&) { return 42; }
size_t size(const my_range&) { return 42; }

}  // nonmember_sized_range

TEST(TestRange, TestNonMemberSize) {
  nonmember_sized_range::my_range a;
  static_assert(parlay::internal::nonmember_size_impl::has_nonmember_size_v<nonmember_sized_range::my_range>);
  static_assert(parlay::internal::nonmember_size_impl::has_nonmember_size_v<nonmember_sized_range::my_range&>);
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 42);
}

TEST(TestRange, TestVector) {
  auto a = std::vector<int>{1,2,3};
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestArray) {
  auto a = std::array<int, 3>{1,2,3};
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestSequence) {
  auto a = parlay::sequence<int>{1,2,3};
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 3);
}

TEST(TestRange, TestDelayedSequence) {
  auto a = parlay::delayed_seq<int>(10, [](int x) { return x; });
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(a), 10);
}

TEST(TestRange, TestSlice) {
  std::vector<int> a = {1,2,3};
  auto s = parlay::make_slice(a);
  static_assert(std::is_unsigned_v<decltype(parlay::size(a))>);
  ASSERT_EQ(parlay::size(s), 3);
}

TEST(TestRange, TestRangeSize) {
  auto a = parlay::delayed_seq<int>(10, [](int x) { return x; });
  parlay::size_of rs;
  ASSERT_EQ(rs(a), 10);
}
