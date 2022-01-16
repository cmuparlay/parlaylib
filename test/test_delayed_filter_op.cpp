#include "gtest/gtest.h"

#include <algorithm>
#include <optional>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/range.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

// Check that filtered ranges are copyable and movable
struct p { std::optional<int> operator()(int) const; };

using si = parlay::sequence<int>;
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<si>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<si>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<si&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<si&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<si>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<si>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<si&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<si&>(), p{}))>);

struct dsi { int operator()(size_t) const; };
using dssi = decltype(parlay::delayed_tabulate(0, std::declval<dsi>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<dssi>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<dssi>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<dssi&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<dssi&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<dssi>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<dssi>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<dssi&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<dssi&>(), p{}))>);

using bdsi = decltype(parlay::block_iterable_wrapper(std::declval<si>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<bdsi>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<bdsi>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter_op(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter_op(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<bdsi>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<bdsi>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter_op(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter_op(std::declval<bdsi&>(), p{}))>);

TEST(TestDelayedFilterOp, TestFilterOpEmpty) {
  const parlay::sequence<int> seq;
  auto f = parlay::delayed::filter_op(seq, [](auto x) { return std::make_optional(x); });

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFilterOp, TestFilterOpAll) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter_op(seq, [](auto) -> std::optional<int> { return std::nullopt; });

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFilterOp, TestFilterOpSimple) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter_op(seq, [](auto x) -> std::optional<int> {
    if (x % 2 == 0) return std::make_optional(x); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(s.begin(), s.end(), answer.begin()));
}

TEST(TestDelayedFilterOp, TestFilterOpConst) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  const auto f = parlay::delayed::filter_op(seq, [](auto x) -> std::optional<int> {
    if (x % 2 == 0) return std::make_optional(x); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(s.begin(), s.end(), answer.begin()));
}

TEST(TestDelayedFilterOp, TestFilterOpNonConst) {
  auto seq = parlay::NonConstRange(100000);
  auto f = parlay::delayed::filter_op(seq, [](auto x) -> std::optional<int> {
    if (x % 2 == 0) return std::make_optional(x); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(s.begin(), s.end(), answer.begin()));
}

TEST(TestDelayedFilterOp, TestFilterOpMutable) {
  auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter_op(seq, [](int& x) -> std::optional<std::reference_wrapper<int>> {
    if (x % 2 == 0) return std::make_optional(std::ref(x)); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  for (int& x : f) {
    x++;
  }

  for (int x : seq) {
    ASSERT_TRUE(x % 2 == 1);
  }
}

TEST(TestDelayedFilterOp, TestFilterOpOwningMutable) {
  auto f = parlay::delayed::filter_op(parlay::to_sequence(parlay::iota<int>(100000)),
            [](int& x) -> std::optional<std::reference_wrapper<int>> {
                if (x % 2 == 0) return std::make_optional(std::ref(x)); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  for (int& x : f) {
    x++;
  }

  for (int x : f) {
    ASSERT_TRUE(x % 2 == 1);
  }
}

TEST(TestDelayedFilterOp, TestFilterOpTemporaries) {
  const auto seq = parlay::iota<int>(100000);
  auto f = parlay::delayed::filter_op(seq, [](auto x) -> std::optional<int> {
    if (x % 2 == 0) return std::make_optional(x); else return std::nullopt; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(f.begin(), f.end(), answer.begin()));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(s.begin(), s.end(), answer.begin()));
}

TEST(TestDelayedFilterOp, TestFilterOpNonTrivialTemporaries) {
  const auto seq = parlay::delayed_tabulate(5000, [](int i) { return std::vector<int>(i); });
  auto f = parlay::delayed::filter_op(seq, [](auto x) -> std::optional<std::vector<int>> {
    if (x.size() % 2 == 0) return std::make_optional(std::move(x)); else return std::nullopt; });
  ASSERT_EQ(f.size(), 2500);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());

  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(s[i].size(), 2*i);
  }
}
