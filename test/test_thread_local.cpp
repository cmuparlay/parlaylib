#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <parlay/thread_specific.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>


TEST(TestThreadLocal, TestUniqueIds) {
  std::vector<std::atomic<bool>> id_used(parlay::num_workers());
  for (size_t i = 0; i < parlay::num_workers(); i++) id_used[i] = false;
  parlay::parallel_for(0, 100000, [&](size_t) {
    size_t id = parlay::my_thread_id();
    ASSERT_LT(id, parlay::num_workers());
    ASSERT_FALSE(id_used[id].exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_EQ(id, parlay::my_thread_id());
    ASSERT_TRUE(id_used[id].exchange(false));
  });
}

TEST(TestThreadLocal, TestThreadLocal) {

  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000000, [&](size_t) {
    *list += 1;
  }, 1);

  int total = 0;
  list.for_each([&](int& x) { total += x; });

  ASSERT_EQ(total, 1000000);
}

TEST(TestThreadLocal, TestThreadLocalCustomConstructor) {

  parlay::ThreadSpecific<int> list([](void* p) { new (p) int{42}; });

  parlay::parallel_for(0, 1000000, [&](size_t) {
    ASSERT_EQ(*list, 42);
  }, 1);

}

TEST(TestThreadLocal, TestThreadLocalDestructor) {

  std::atomic<int> constructions{0};
  std::atomic<int> destructions{0};

  {
    struct MyType {
      explicit MyType(std::atomic<int>& constructions_, std::atomic<int>& destructions_)
          : destructions(destructions_) {
        constructions_.fetch_add(1);
      }
      ~MyType() { destructions.fetch_add(1); }
      std::atomic<int>& destructions;
    };

    parlay::ThreadSpecific<MyType> list([&](void* p) { new (p) MyType{constructions, destructions}; });

    parlay::parallel_for(0, 1000000, [&](size_t) {
      ASSERT_EQ(list->destructions.load(), 0);
    }, 1);
  }

  ASSERT_GE(constructions.load(), parlay::num_thread_ids());
  ASSERT_EQ(constructions.load(), destructions.load());
}

TEST(TestThreadLocal, TestThreadLocalUnique) {

  // Make sure the atomic<bool>s are initialized to false.
  parlay::ThreadSpecific<std::atomic<bool>> list([](void* p) { new (p) std::atomic<bool>{false}; });

  parlay::parallel_for(0, 100000, [&](size_t) {
    ASSERT_FALSE(list->exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_TRUE(list->exchange(false));
  });
}

TEST(TestThreadLocal, TestThreadLocalIterate) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
  }, 1);

  int tid = 0;

  for (int x : list) {
    ASSERT_EQ(x, tid++);
  }

}

TEST(TestThreadLocal, TestParallelIterate) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
  }, 1);

  parlay::for_each(list, [](int x) {
    ASSERT_GE(x, 0);
    ASSERT_LT(x, parlay::num_thread_ids());
  });
}
