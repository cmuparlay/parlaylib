#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <thread>
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

TEST(TestParallel, TestParDoOnlyOnce) {
  std::atomic<bool> f1(false), f2(false);
  parlay::par_do(
      [&]() { ASSERT_FALSE(f1.exchange(true)); },
      [&]() { ASSERT_FALSE(f2.exchange(true)); }
  );
  ASSERT_TRUE(f1.load());
  ASSERT_TRUE(f2.load());
}

template<typename F>
void simulated_for(size_t start, size_t end, F&& f) {
  if (end == start + 1) f(start);
  else {
    size_t mid = start + (end - start) / 2;
    parlay::par_do(
      [&]() { simulated_for(start, mid, f); },
      [&]() { simulated_for(mid, end, f); }
    );
  }
}

TEST(TestParallel, TestParDoWorkerIds) {
  std::vector<std::atomic<bool>> id_used(parlay::num_workers());
  for (size_t i = 0; i < parlay::num_workers(); i++) id_used[i] = false;
  simulated_for(0, 100000, [&](size_t) {
    size_t id = parlay::worker_id();
    ASSERT_FALSE(id_used[id].exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_EQ(id, parlay::worker_id());
    ASSERT_TRUE(id_used[id].exchange(false));
  });
}

TEST(TestParallel, TestParDoUncopyableF) {
  struct F {
    F() = default;
    F(const F&) = delete;
    F(F&&) = default;
    void operator()() const { }
  };
  parlay::par_do(F{}, F{});
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

TEST(TestParallel, TestParForRef) {
  size_t n = 100000;
  std::vector<int> v(n);
  auto f = [&](auto i) { v[i] = i; };
  parlay::parallel_for(0, n, f);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}

TEST(TestParallel, TestParForMovedF) {
  size_t n = 100000;
  std::vector<int> v1(n), v2(n);
  for (size_t i = 0; i < n; i++) v1[i] = i;
  struct F {
    void operator()(int i) { v2[i] = v1[i]; }
    std::vector<int> v1;
    std::vector<int>& v2;
  };
  parlay::parallel_for(0, n, F{std::move(v1), v2});
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v2[i], i);
  }
}

TEST(TestParallel, TestParForUncopyableF) {
  size_t n = 100000;
  std::vector<int> v(n);
  struct F {
    F(std::vector<int>& v_) : v(v_) {}
    F(const F&) = delete;
    std::vector<int>& v;
    void operator()(size_t i) const { v[i] = i; } };
  F f{v};
  parlay::parallel_for(0, n, f);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}

TEST(TestParallel, TestParForUncopyableTempF) {
  size_t n = 100000;
  std::vector<int> v(n);
  struct F {
    F(std::vector<int>& v_) : v(v_) {}
    F(const F&) = delete;
    std::vector<int>& v;
    void operator()(size_t i) const { v[i] = i; } };
  parlay::parallel_for(0, n, F{v});
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}

TEST(TestParallel, TestParForOnlyOnce) {
  size_t n = 100000;
  std::vector<std::atomic<bool>> v(n);
  for (size_t i = 0; i < n; i++) v[i] = false;
  parlay::parallel_for(0, n, [&](size_t i) {
    ASSERT_FALSE(v[i].exchange(true));
  });
}

TEST(TestParallel, TestParForWorkerIds) {
  std::vector<std::atomic<bool>> id_used(parlay::num_workers());
  for (size_t i = 0; i < parlay::num_workers(); i++) id_used[i] = false;
  parlay::parallel_for(0, 100000, [&](size_t) {
    size_t id = parlay::worker_id();
    ASSERT_FALSE(id_used[id].exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_EQ(id, parlay::worker_id());
    ASSERT_TRUE(id_used[id].exchange(false));
  });
}

TEST(TestParallel, TestNestedParForWorkerIds) {
  std::vector<std::atomic<bool>> id_used(parlay::num_workers());
  for (size_t i = 0; i < parlay::num_workers(); i++) id_used[i] = false;
  parlay::parallel_for(0, 200, [&](size_t) {
    parlay::parallel_for(0, 200, [&](size_t) {
      size_t id = parlay::worker_id();
      ASSERT_FALSE(id_used[id].exchange(true));
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      ASSERT_EQ(id, parlay::worker_id());
      ASSERT_TRUE(id_used[id].exchange(false));
    });
  });
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
