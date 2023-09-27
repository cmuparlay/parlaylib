#include "gtest/gtest.h"

#include <utility>

#include <parlay/parallel.h>

#include <parlay/internal/concurrency/big_atomic.h>

struct ManyLongs {
    long long x, y, z;
    bool operator==(const ManyLongs& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
};

static_assert(sizeof(ManyLongs) == 24);
static_assert(std::is_trivially_copyable_v<ManyLongs>);

TEST(TestBigAtomic, TestDefaultContruct) {
    parlay::big_atomic<ManyLongs> ba;
}

TEST(TestBigAtomic, TestRead) {
  parlay::big_atomic<ManyLongs> ba({1,2,3});
  auto val = ba.load();
  ASSERT_EQ(val, ManyLongs({1,2,3}));
}

TEST(TestBigAtomic, TestStore) {
  parlay::big_atomic<ManyLongs> ba({1,2,3});
  auto val = ba.load();
  ASSERT_EQ(val, ManyLongs({1,2,3}));

  ba.store({4,5,6});
  val = ba.load();
  ASSERT_EQ(val, ManyLongs({4,5,6}));
}

TEST(TestBigAtomic, TestCasSuccess) {
  parlay::big_atomic<ManyLongs> ba({1,2,3});
  auto expected = ba.load();
  ASSERT_TRUE(ba.compare_and_swap(expected, ManyLongs{4,5,6}));
  auto val = ba.load();
  ASSERT_EQ(val, ManyLongs({4,5,6}));
}

TEST(TestBigAtomic, TestCasFailure) {
  parlay::big_atomic<ManyLongs> ba({1,2,3});
  auto expected = ba.load();
  expected.x = 2;
  ASSERT_FALSE(ba.compare_and_swap(expected, ManyLongs{4,5,6}));
  auto val = ba.load();
  ASSERT_EQ(val, ManyLongs({1,2,3}));
}

TEST(TestBigAtomic, TestConcurrent) {
  std::atomic<long long> total1{0}, total2{0};
  parlay::big_atomic<ManyLongs> ba({0,0,0});

  parlay::execute_with_scheduler(128, [&]() {
    parlay::parallel_for(0, 100000, [&](long long i) {
      auto val = ba.load();
      auto new_val = ManyLongs{i, 2 * i, 3 * i};
      if (ba.compare_and_swap(val, new_val)) {
        total1.fetch_add(val.x + val.y + val.z);
        total2.fetch_add(new_val.x + new_val.y + new_val.z);
      }
    }, 1);
  });

  auto last = ba.load();
  ASSERT_EQ(total1.load() + last.x + last.y + last.z, total2.load());
}
