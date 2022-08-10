#include "gtest/gtest.h"

#include <atomic>
#include <vector>

#include <parlay/alloc.h>
#include <parlay/parallel.h>


TEST(TestParallel, TestParDo) {
  int x = 0, y = 0;
  parlay::par_do(
      [&]() { x = 1; },
      [&]() { y = 2; }
  );
  ASSERT_EQ(x, 1);
  ASSERT_EQ(y, 2);
}

TEST(TestParallel, TestParDoSafeRace) {
  std::atomic<int> x;
  parlay::par_do(
      [&]() { x = 1; },
      [&]() { x = 2; }
  );
  ASSERT_TRUE(x == 1 || x == 2);
}

TEST(TestParallel, TestParFor) {
  size_t n = 100000;
  std::vector<int> v(n);
  parlay::parallel_for(0, n, [&](auto i) {
    v[i] = i;
  });
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}

TEST(TestParallel, TestNestedParDo) {
  int a, b, c, d;
  parlay::par_do(
    [&]() { parlay::par_do([&]() { a = 1; }, [&]() { b = 2; }); },
    [&]() { parlay::par_do([&]() { c = 3; }, [&]() { d = 4; }); }
  );
  ASSERT_EQ(a, 1);
  ASSERT_EQ(b, 2);
  ASSERT_EQ(c, 3);
  ASSERT_EQ(d, 4);
}

TEST(TestParallel, TestNestedParFor) {
  constexpr size_t n = 1000;
  std::vector<std::vector<int>> A(n, std::vector<int>(n));
  parlay::parallel_for(0, n, [&](size_t i) {
    parlay::parallel_for(0, n, [&](size_t j) {
      A[i][j] = i*j;
    });
  });
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      ASSERT_EQ(A[i][j], i*j);
    }
  }
}

TEST(TestParallel, TestParDoInsideFor) {
  size_t n = 100000;
  std::vector<int> v1(n), v2(n);
  parlay::parallel_for(0, n, [&](auto i) {
    parlay::par_do(
      [&]() { v1[i] = i; },
      [&]() { v2[i] = i; }
    );
  });
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v1[i], i);
    ASSERT_EQ(v2[i], i);
  }
}

TEST(TestParallel, TestParForInsideDo) {
  size_t n = 100000;
  std::vector<int> v1(n), v2(n);
  parlay::par_do(
    [&]() { parlay::parallel_for(0, n, [&](auto i) { v1[i] = i; }); },
    [&]() { parlay::parallel_for(0, n, [&](auto i) { v2[i] = i; }); }
  );
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v1[i], i);
    ASSERT_EQ(v2[i], i);
  }
}

TEST(TestParallel, TestNestedAlloc) {
  parlay::parallel_for(0, 10000, [](size_t) {
    parlay::parallel_for(0, 10000, [](size_t) {
      int* x = parlay::type_allocator<int>::alloc();
      ASSERT_NE(x, nullptr);
      parlay::type_allocator<int>::free(x);
    });
  });
}
