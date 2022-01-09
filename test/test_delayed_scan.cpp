#include "gtest/gtest.h"

#include <algorithm>
#include <functional>
#include <numeric>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed_views.h>

#include "range_utils.h"

// Check that scanned ranges are copyable and movable
using si = parlay::sequence<int>;
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan(std::declval<si>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan(std::declval<si>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan(std::declval<si&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan(std::declval<si&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan(std::declval<si>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan(std::declval<si>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan(std::declval<si&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan(std::declval<si&>()))>);

static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<si>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<si>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<si&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<si&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<si>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<si>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<si&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<si&>()))>);

using bdsi = decltype(parlay::block_iterable_wrapper(std::declval<si>()));
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan(std::declval<bdsi>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan(std::declval<bdsi>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan(std::declval<bdsi&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan(std::declval<bdsi&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan(std::declval<bdsi>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan(std::declval<bdsi>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan(std::declval<bdsi&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan(std::declval<bdsi&>()))>);

static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi>()))>);
static_assert(std::is_copy_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi&>()))>);
static_assert(std::is_copy_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi&>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi>()))>);
static_assert(std::is_move_constructible_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi&>()))>);
static_assert(std::is_move_assignable_v<decltype(parlay::delayed::scan_inclusive(std::declval<bdsi&>()))>);


// ---------------------------------------------------------------------------------------
//                                     BID VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedScan, TestScanEmpty) {
  const parlay::sequence<int> a;
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, 0);
  ASSERT_EQ(m.begin(), m.end());

  auto s = parlay::delayed::to_sequence(m);
  ASSERT_TRUE(s.empty());
}

TEST(TestDelayedScan, TestScanSimple) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, 1800030000);

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res += a[i];
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanToSeq) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, 1800030000);

  auto s = parlay::delayed::to_sequence(m);
  ASSERT_EQ(s.size(), m.size());

  int res = 0;
  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(res, s[i]);
    res += a[i];
  }
}

TEST(TestDelayedScan, TestScanSimpleOwning) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(parlay::iota(60001));
  auto [m, total] = parlay::delayed::scan(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, 1800030000);

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res += a[i];
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanConstRef) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, 1800030000);

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res += a[i];
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveEmpty) {
  const parlay::sequence<int> a;
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(m.begin(), m.end());

  auto s = parlay::delayed::to_sequence(m);
  ASSERT_TRUE(s.empty());
}

TEST(TestDelayedScan, TestScanInclusiveSimple) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid);

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    res += a[i];
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveToSeq) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid);

  ASSERT_EQ(m.size(), a.size());

  auto s = parlay::delayed::to_sequence(m);
  ASSERT_EQ(s.size(), m.size());

  int res = 0;
  for (size_t i = 0; i < s.size(); i++) {
    res += a[i];
    ASSERT_EQ(res, s[i]);
  }
}

TEST(TestDelayedScan, TestScanInclusiveSimpleOwning) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(parlay::iota(60001));
  auto m = parlay::delayed::scan_inclusive(bid);

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    res += a[i];
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveConstRef) {
  parlay::sequence<int> a = parlay::to_sequence(parlay::iota(60001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid);

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    res += a[i];
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanCustomOp) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid, std::bit_xor<>{});

  auto actual_total = std::accumulate(std::begin(a), std::end(a), 0, std::bit_xor<>{});

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, actual_total);

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res ^= a[i];
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveCustomOp) {
  const parlay::sequence<int> a = parlay::to_sequence(parlay::iota(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid, std::bit_xor<>{});

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  int res = 0;
  for (size_t i = 0; i < m.size(); i++) {
    res ^= a[i];
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanCustomIdentity) {
  const parlay::sequence<unsigned int> a = parlay::to_sequence(parlay::iota<unsigned int>(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid, std::multiplies<>{}, 1U);

  auto actual_total = std::accumulate(std::begin(a), std::end(a), 1U, std::multiplies<>{});

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, actual_total);

  auto it = m.begin();
  unsigned int res = 1U;
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res *= a[i];
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveCustomIdentity) {
  const parlay::sequence<unsigned int> a = parlay::to_sequence(parlay::iota<unsigned int>(100001));
  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid, std::multiplies<>{}, 1U);

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  unsigned int res = 1U;
  for (size_t i = 0; i < m.size(); i++) {
    res *= a[i];
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

template<typename T, size_t N>
struct BasicMatrix {

  friend bool operator==(const BasicMatrix& A, const BasicMatrix& B) {
    for (size_t i = 0; i < 3; i++) {
      for (size_t j = 0; j < 3; j++) {
        if (A(i, j) != B(i, j)) return false;
      }
    }
    return true;
  }

  T& operator()(size_t i, size_t j) { return m[i][j]; }
  const T& operator()(size_t i, size_t j) const { return m[i][j]; }

  BasicMatrix() : m(N, std::vector<T>(N)) {}

  static BasicMatrix zero() { return {}; }

 private:
  std::vector<std::vector<T>> m;
};

auto matrix_add(BasicMatrix<int, 3> a, const BasicMatrix<int, 3>& b) {
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < 3; j++) {
      a(i, j) += b(i, j);
    }
  }
  return a;
};

TEST(TestDelayedScan, TestScanCustomType) {
  const auto a = parlay::tabulate(50000, [&](size_t i) {
    BasicMatrix<int, 3> m;
    for (size_t j = 0; j < 3; j++) {
      for (size_t k = 0; k < 3; k++) {
        m(j, k) = i + j + k;
      }
    }
    return m;
  });

  const auto bid = parlay::block_iterable_wrapper(a);
  auto [m, total] = parlay::delayed::scan(bid, matrix_add, BasicMatrix<int,3>::zero());

  auto actual_total = std::accumulate(std::begin(a), std::end(a), BasicMatrix<int,3>::zero(), matrix_add);

  ASSERT_EQ(m.size(), a.size());
  ASSERT_EQ(total, actual_total);

  auto it = m.begin();
  auto res = BasicMatrix<int,3>::zero();
  for (size_t i = 0; i < m.size(); i++) {
    ASSERT_EQ(res, *it);
    res = matrix_add(std::move(res), a[i]);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}

TEST(TestDelayedScan, TestScanInclusiveCustomType) {
  const auto a = parlay::tabulate(50000, [&](size_t i) {
    BasicMatrix<int, 3> m;
    for (size_t j = 0; j < 3; j++) {
      for (size_t k = 0; k < 3; k++) {
        m(j, k) = i + j + k;
      }
    }
    return m;
  });

  const auto bid = parlay::block_iterable_wrapper(a);
  auto m = parlay::delayed::scan_inclusive(bid, matrix_add, BasicMatrix<int,3>::zero());

  ASSERT_EQ(m.size(), a.size());

  auto it = m.begin();
  auto res = BasicMatrix<int,3>::zero();
  for (size_t i = 0; i < m.size(); i++) {
    res = matrix_add(std::move(res), a[i]);
    ASSERT_EQ(res, *it);
    ++it;
  }
  ASSERT_EQ(it, m.end());
}
