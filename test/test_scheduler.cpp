#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>

#include <parlay/scheduler.h>

void go_sleep(unsigned int t) {
  std::this_thread::sleep_for(std::chrono::milliseconds(t));
}

TEST(TestScheduler, TestInitialise) {
  parlay::fork_join_scheduler fj;
}

TEST(TestScheduler, TestParDo) {
  parlay::fork_join_scheduler fj;
  int x = 0, y = 0;
  fj.pardo(
    [&]() { x = 1; },
    [&]() { y = 2; }
  );
  ASSERT_EQ(x, 1);
  ASSERT_EQ(y, 2);
}

TEST(TestScheduler, TestAtomicRace) {
  parlay::fork_join_scheduler fj;
  std::atomic<int> x = 5;
  fj.pardo(
    [&x]() { x.store(1); go_sleep(50); },
    [&x]() { x.store(2); go_sleep(50); }
  );
  ASSERT_TRUE(x == 1 || x == 2);
}

template<typename It>
long long int simple_reduce(parlay::fork_join_scheduler& fj, It begin, It end) {
  if (begin == end - 1) {
    return *begin;
  }
  else {
    auto mid = begin + std::distance(begin, end) / 2;
    long long int left, right;
    fj.pardo(
      [&]() { left = simple_reduce(fj, begin, mid); },
      [&]() { right = simple_reduce(fj, mid, end); }
    );
    return left + right;
  }
}

TEST(TestScheduler, TestReduce) {
  size_t n = 1000;
  parlay::fork_join_scheduler fj;
  std::vector<int> v(n);
  std::iota(std::begin(v), std::end(v), 0);
  auto sum = std::accumulate(std::begin(v), std::end(v), 0LL);
  auto computed = simple_reduce(fj, std::begin(v), std::end(v));
  ASSERT_EQ(sum, computed);
}

TEST(TestScheduler, TestParFor) {
  size_t n = 1000;
  parlay::fork_join_scheduler fj;
  std::vector<int> v(n);
  fj.parfor(0, n, [&](auto i) {
    v[i] = i;
  });
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}
