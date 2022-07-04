#include "gtest/gtest.h"

#include <parlay/internal/concurrency/hp_stack.h>

TEST(TestHpStack, TestConstruction) {
  parlay::internal::concurrency::hp_stack<int> stack;
}

TEST(TestHpStack, TestPush) {
  parlay::internal::concurrency::hp_stack<int> stack;
  stack.push(1);
}

TEST(TestHpStack, TestPop) {
  parlay::internal::concurrency::hp_stack<int> stack;
  stack.push(1);
  auto x = stack.pop();
  ASSERT_TRUE(x.has_value());
  ASSERT_EQ(x.value(), 1);
}

TEST(TestHpStack, TestEmpty) {
  parlay::internal::concurrency::hp_stack<int> stack;
  ASSERT_TRUE(stack.empty());
  stack.push(1);
  ASSERT_FALSE(stack.empty());
  stack.pop();
  ASSERT_TRUE(stack.empty());
}

TEST(TestHpStack, TestSize) {
  parlay::internal::concurrency::hp_stack<int> stack;
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(stack.size(), i);
    stack.push(0);
  }
}

TEST(TestHpStack, TestClear) {
  parlay::internal::concurrency::hp_stack<int> stack;
  stack.push(1);
  stack.clear();
  ASSERT_TRUE(stack.empty());
}

TEST(TestHpStack, TestSequential) {
  parlay::internal::concurrency::hp_stack<int> stack;
  ASSERT_EQ(stack.size(), 0);
  ASSERT_TRUE(stack.empty());
  ASSERT_TRUE(!stack.pop());
  stack.push(5);
  ASSERT_FALSE(stack.empty());
  ASSERT_EQ(stack.size(), 1);
  ASSERT_TRUE(stack.pop().value() == 5);
  ASSERT_TRUE(stack.empty());
  ASSERT_EQ(stack.size(), 0);
  stack.push(5);
  stack.push(6);
  stack.push(7);
  ASSERT_FALSE(stack.empty());
  ASSERT_EQ(stack.size(), 3);
  ASSERT_TRUE(stack.pop().value() == 7);
  ASSERT_TRUE(stack.pop().value() == 6);
  ASSERT_TRUE(stack.pop().value() == 5);
}

TEST(TestHpStack, TestParallel) {
  constexpr int M = 100000;

  long long checksum1 = 0;
  long long checksum2 = 0;
  long long actualsum1 = 0;
  long long actualsum2 = 0;
  
  parlay::internal::concurrency::hp_stack<int> stack;
  std::atomic<bool> done1 = false;
  std::atomic<bool> done2 = false;

  std::atomic<int> tid(1);

  parlay::parallel_for(0, 4, [&](size_t) {
    int t = tid.fetch_add(1);
    if (t == 1) {
      for(int i = 0; i < M; i++) {
        stack.push(i);
        actualsum1 += i;
      }
      done1 = true;
    }
    else if (t == 2) {
      for(int i = 0; i < M; i++) {
        stack.push(i);
        actualsum2 += i;
      }
      done2 = true;
    }
    else if (t == 3) {
      while(!done1 || !done2 || !stack.empty()) {
        auto val = stack.pop();
        if (val) checksum1 += val.value();
      }
    }
    else {
      while(!done1 || !done2 || !stack.empty()) {
        auto val = stack.pop();
        if (val) checksum2 += val.value();
      }
    }
  }, 1);

  ASSERT_TRUE(stack.empty());
  ASSERT_EQ(checksum1 + checksum2, actualsum1 + actualsum2);
}
