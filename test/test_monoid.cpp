#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <random>
#include <string>

#include <parlay/monoid.h>
#include <parlay/primitives.h>

template<typename T, size_t N>
struct BasicMatrix {

  friend bool operator==(const BasicMatrix& A, const BasicMatrix& B) {
    for (size_t i = 0; i < N; i++) {
      for (size_t j = 0; j < N; j++) {
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

template<size_t N>
auto matrix_add(BasicMatrix<int, N> a, const BasicMatrix<int, N>& b) {
  for (size_t i = 0; i < N; i++) {
    for (size_t j = 0; j < N; j++) {
      a(i, j) += b(i, j);
    }
  }
  return a;
};

auto get_matrices() {
  return parlay::tabulate(50000, [&](size_t i) {
    BasicMatrix<int, 3> m;
    for (size_t j = 0; j < 3; j++) {
      for (size_t k = 0; k < 3; k++) {
        m(j, k) = i + j + k;
      }
    }
    return m;
  });
}

TEST(TestMonoid, TestBinaryOpWithLambda) {
  const auto a = get_matrices();
  auto add = [](auto&& x, auto&& y) { return matrix_add<3>(x,y); };
  auto matrix3_add = parlay::binary_op(add, BasicMatrix<int,3>::zero());

  auto x = parlay::reduce(a, matrix3_add);
  auto actual_total = std::accumulate(std::begin(a), std::end(a), BasicMatrix<int,3>::zero(), add);
  static_assert(std::is_same_v<decltype(x), BasicMatrix<int,3>>);
  ASSERT_EQ(x, actual_total);
}

TEST(TestMonoid, TestBinaryOpWithFnPointer) {
  const auto a = get_matrices();
  auto matrix3_add = parlay::binary_op(matrix_add<3>, BasicMatrix<int,3>::zero());

  auto x = parlay::reduce(a, matrix3_add);
  auto actual_total = std::accumulate(std::begin(a), std::end(a), BasicMatrix<int,3>::zero(), matrix_add<3>);
  static_assert(std::is_same_v<decltype(x), BasicMatrix<int,3>>);
  ASSERT_EQ(x, actual_total);
}

TEST(TestMonoid, TestBinaryOpWithClass) {
  const auto a = get_matrices();

  struct Adder {
    auto operator()(BasicMatrix<int,3> a, const BasicMatrix<int,3>& b) const {
      return matrix_add(std::move(a), b);
    }
  };

  auto matrix3_add = parlay::binary_op(Adder{}, BasicMatrix<int,3>::zero());

  auto x = parlay::reduce(a, matrix3_add);
  auto actual_total = std::accumulate(std::begin(a), std::end(a), BasicMatrix<int,3>::zero(), matrix_add<3>);
  static_assert(std::is_same_v<decltype(x), BasicMatrix<int,3>>);
  ASSERT_EQ(x, actual_total);
}
