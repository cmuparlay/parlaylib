#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <parlay/thread_local.h>
#include <parlay/parallel.h>

TEST(TestThreadLocal, TestUniqueIds) {
  std::vector<std::atomic<bool>> id_used(10000);
  for (size_t i = 0; i < 10000; i++) id_used[i] = false;
  parlay::parallel_for(0, 100000, [&](size_t) {
    size_t id = parlay::my_thread_id();
    ASSERT_FALSE(id_used[id].exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_EQ(id, parlay::my_thread_id());
    ASSERT_TRUE(id_used[id].exchange(false));
  });
}
TEST(TestThreadLocal, TestThreadLocal) {

}
