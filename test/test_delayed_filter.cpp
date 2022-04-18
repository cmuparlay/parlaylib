#include "gtest/gtest.h"

#include <algorithm>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/range.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

// Check that filtered ranges are copyable and movable
struct p { bool operator()(int) const; };

using si = parlay::sequence<int>;
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<si>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<si>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<si&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<si&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<si>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<si>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<si&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<si&>(), p{}))>);

struct dsi { int operator()(size_t) const; };
using dssi = decltype(parlay::delayed_tabulate(0, std::declval<dsi>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<dssi>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<dssi>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<dssi&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<dssi&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<dssi>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<dssi>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<dssi&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<dssi&>(), p{}))>);

using bdsi = decltype(parlay::block_iterable_wrapper(std::declval<si>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<bdsi>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<bdsi>(), p{}))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::filter(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::filter(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<bdsi>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<bdsi>(), p{}))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::filter(std::declval<bdsi&>(), p{}))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::filter(std::declval<bdsi&>(), p{}))>);

TEST(TestDelayedFilter, TestFilterEmpty) {
  const parlay::sequence<int> seq;
  auto f = parlay::delayed::filter(seq, [](auto) { return true; });

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFilter, TestFilterAll) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter(seq, [](auto x) { return x >= 100000; });

  ASSERT_EQ(f.size(), 0);
  ASSERT_EQ(f.begin(), f.end());
  ASSERT_EQ(f.get_num_blocks(), 0);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), 0);
}

TEST(TestDelayedFilter, TestFilterSimple) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter(seq, [](auto x) { return x % 2 == 0; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(std::begin(f), std::end(f), std::begin(answer)));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(answer)));
}

TEST(TestDelayedFilter, TestFilterConst) {
  const auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  const auto f = parlay::delayed::filter(seq, [](auto x) { return x % 2 == 0; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(std::begin(f), std::end(f), std::begin(answer)));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(answer)));
}

TEST(TestDelayedFilter, TestFilterNonConst) {
  auto seq = parlay::NonConstRange(100000);
  auto f = parlay::delayed::filter(seq, [](auto x) { return x % 2 == 0; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(std::begin(f), std::end(f), std::begin(answer)));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(answer)));
}

TEST(TestDelayedFilter, TestFilterMutable) {
  auto seq = parlay::to_sequence(parlay::iota<int>(100000));
  auto f = parlay::delayed::filter(seq, [](auto x) { return x % 2 == 0; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(std::begin(f), std::end(f), std::begin(answer)));

  for (int& x : f) {
    x++;
  }

  for (int x : seq) {
    ASSERT_TRUE(x % 2 == 1);
  }
}

TEST(TestDelayedFilter, TestFilterTemporaries) {
  const auto seq = parlay::iota<int>(100000);
  auto f = parlay::delayed::filter(seq, [](auto x) { return x % 2 == 0; });
  auto answer = parlay::map(parlay::iota(50000), [](int x) { return 2*x; });

  ASSERT_EQ(f.size(), 50000);
  ASSERT_TRUE(std::equal(std::begin(f), std::end(f), std::begin(answer)));

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());
  ASSERT_TRUE(std::equal(std::begin(s), std::end(s), std::begin(answer)));
}

TEST(TestDelayedFilter, TestFilterNonTrivialTemporaries) {
  const auto seq = parlay::delayed_tabulate(5000, [](int i) { return std::vector<int>(i); });
  auto f = parlay::delayed::filter(seq, [](auto x) { return x.size() % 2 == 0; });
  ASSERT_EQ(f.size(), 2500);

  auto s = parlay::delayed::to_sequence(f);
  ASSERT_EQ(s.size(), f.size());

  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(s[i].size(), 2*i);
  }
}

TEST(TestDelayedFilter, TestFilterRValueRefs) {
  auto s = parlay::tabulate(5000, [](size_t i) {
    auto res = std::vector<int>(i);
    std::iota(res.begin(), res.end(), 0);
    return res;
  });

  // Map the contents of s to rvalue references => they should be moved into the copy
  auto m = parlay::delayed_map(s, [](std::vector<int>& x) -> std::vector<int>&& { return std::move(x); });

  auto f = parlay::delayed::filter(m, [](auto&& v) { return v.size() % 2 == 0; } );
  ASSERT_EQ(f.size(), 2500);

  // Should move from the input due to rvalue references
  auto seq = parlay::delayed::to_sequence(f);
  ASSERT_EQ(seq.size(), 2500);
  for (size_t i = 0; i < seq.size(); i++) {
    ASSERT_EQ(seq[i].size(), 2*i);
  }

  for (size_t i = 0; i < s.size(); i++) {
    if (i % 2 == 0) ASSERT_TRUE(s[i].empty());
    else ASSERT_TRUE(s[i].size() == i);
  }
}

TEST(TestDelayedFilter, TestFilterCopyConstruct) {
  auto strings = parlay::tabulate(10000, [](int i) { return std::vector<char>(i, 'a'); });
  auto pred = [](const std::vector<char>& x) { return x.size() % 2 == 0; };
  auto answer = parlay::tabulate(5000, [](int i) { return std::vector<char>(2*i, 'a'); });

  // Create a delayed filter, then copy-construct and return the copy. the original should be
  // destroyed so if we accidentally take iterators from the old sequence they will dangle
  auto f = [&]() {
    auto f = parlay::delayed::filter(parlay::block_iterable_wrapper(strings.to_vector()), pred);
    auto f2 = f;
    return f2;
  }();

  ASSERT_EQ(f.size(), 5000);
  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, answer[i]);
    ++it;
  }
}

TEST(TestDelayedFilter, TestFilterCopyAssign) {
  auto strings = parlay::tabulate(10000, [](int i) { return std::vector<char>(i, 'a'); });
  auto strings2 = parlay::tabulate(10000, [](int i) { return std::vector<char>(i, 'b'); });
  auto pred = [](const std::vector<char>& x) { return x.size() % 2 == 0; };
  auto answer = parlay::tabulate(5000, [](int i) { return std::vector<char>(2*i, 'a'); });
  auto answer2 = parlay::tabulate(5000, [](int i) { return std::vector<char>(2*i, 'b'); });

  // Create a delayed filter, then copy-assign and return the copy. the original should be
  // destroyed so if we accidentally take iterators from the old sequence they will dangle
  auto f = [&]() {
    auto f = parlay::delayed::filter(parlay::block_iterable_wrapper(strings.to_vector()), pred);
    auto f2 = parlay::delayed::filter(parlay::block_iterable_wrapper(strings2.to_vector()), pred);
    f2 = f;
    return f2;
  }();

  ASSERT_EQ(f.size(), 5000);
  auto it = f.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, answer[i]);
    ++it;
  }
}

TEST(TestDelayedFilter, TestFilterSwap) {
  auto strings = parlay::tabulate(10000, [](int i) { return std::vector<char>(i, 'a'); });
  auto strings2 = parlay::tabulate(10000, [](int i) { return std::vector<char>(i, 'b'); });
  auto pred = [](const std::vector<char>& x) { return x.size() % 2 == 0; };
  auto answer = parlay::tabulate(5000, [](int i) { return std::vector<char>(2*i, 'a'); });
  auto answer2 = parlay::tabulate(5000, [](int i) { return std::vector<char>(2*i, 'b'); });

  auto f = parlay::delayed::filter(parlay::block_iterable_wrapper(strings.to_vector()), pred);
  auto f2 = parlay::delayed::filter(parlay::block_iterable_wrapper(strings2.to_vector()), pred);

  ASSERT_EQ(f.size(), 5000);
  ASSERT_EQ(f2.size(), 5000);

  using std::swap;
  swap(f, f2);

  auto it = f.begin();
  auto it2 = f2.begin();
  for (size_t i = 0; i < f.size(); i++) {
    ASSERT_EQ(*it, answer2[i]);
    ASSERT_EQ(*it2, answer[i]);
    ++it;
    ++it2;
  }
}
