#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <parlay/slice.h>
#include <parlay/worker_specific.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>


TEST(TestWorkerSpecific, TestUniqueIds) {
  std::vector<std::atomic<bool>> id_used(parlay::num_workers());
  for (size_t i = 0; i < parlay::num_workers(); i++) id_used[i] = false;
  parlay::parallel_for(0, 100000, [&](size_t) {
    size_t id = parlay::worker_id();
    ASSERT_LT(id, parlay::num_workers());
    ASSERT_FALSE(id_used[id].exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_EQ(id, parlay::worker_id());
    ASSERT_TRUE(id_used[id].exchange(false));
  });
}

TEST(TestWorkerSpecific, TestWorkerSpecific) {

  parlay::WorkerSpecific<int> list;

  parlay::parallel_for(0, 1000000, [&](size_t) {
    *list += 1;
  }, 1);

  int total = 0;
  list.for_each([&](int& x) { total += x; });

  ASSERT_EQ(total, 1000000);
}

TEST(TestWorkerSpecific, TestWorkerSpecificCustomConstructor) {

  parlay::WorkerSpecific<int> list([]() { return 42; });

  parlay::parallel_for(0, 1000000, [&](size_t) {
    ASSERT_EQ(*list, 42);
  }, 1);
}

TEST(TestWorkerSpecific, TestWorkerSpecificCustomConstructorParam) {

  parlay::WorkerSpecific<int> list([](std::size_t tid) { return tid; });

  parlay::parallel_for(0, 1000000, [&](size_t) {
    ASSERT_EQ(*list, parlay::worker_id());
  }, 1);
}

TEST(TestWorkerSpecific, TestWorkerSpecificDestructor) {

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
      volatile int x{0};
    };

    parlay::ThreadSpecific<MyType> list([&]() { return MyType{constructions, destructions}; });

    parlay::parallel_for(0, 1000, [&](size_t) {
      list->x++;
      std::this_thread::sleep_for (std::chrono::milliseconds(10));
    }, 1);
  }

  ASSERT_EQ(constructions.load(), destructions.load());
}

TEST(TestWorkerSpecific, TestWorkerSpecificUnique) {

  // Make sure the atomic<bool>s are initialized to false.
  parlay::WorkerSpecific<std::atomic<bool>> list([]() { return std::atomic<bool>{false}; });

  parlay::parallel_for(0, 100000, [&](size_t) {
    ASSERT_FALSE(list->exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_TRUE(list->exchange(false));
  });
}

TEST(TestWorkerSpecific, TestWorkerSpecificConst) {
  parlay::WorkerSpecific<int> list;

  *list = 42;
  const auto& clist = list;
  ASSERT_EQ(*clist, 42);
}

TEST(TestWorkerSpecific, TestWorkerSpecificIterate) {
  parlay::WorkerSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  int tid = 0;

  for (int x : list) {
    ASSERT_TRUE((x == tid++) || (x == -1));
  }
}

TEST(TestWorkerSpecific, TestWorkerSpecificConstIterate) {
  parlay::WorkerSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  int tid = 0;

  const auto& clist = list;

  for (int x : clist) {
    ASSERT_TRUE((x == tid++) || (x == -1));
  }
}

TEST(TestWorkerSpecific, TestWorkerSpecificIterateReverse) {
  parlay::WorkerSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  int tid = (int)parlay::num_workers();

  auto rev = parlay::make_slice(std::make_reverse_iterator(list.end()), std::make_reverse_iterator(list.begin()));

  for (int x : rev) {
    ASSERT_TRUE((x == --tid) || (x == -1));
  }
}

TEST(TestWorkerSpecific, TestWorkerSpecificIterateInitialize) {
  parlay::WorkerSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::worker_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  // Ensure that the list values are initialized
  for (int x : list) {
    ASSERT_EQ(x, 42);
  }
}

TEST(TestWorkerSpecific, TestWorkerSpecificRandomAccessIterator) {
  parlay::WorkerSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  auto it = list.begin();

  for (unsigned p = 0; p < parlay::num_workers(); p++) {
    auto val = it[p];
    ASSERT_TRUE((val == (int)p) || (val == -1));
  }

}

TEST(TestWorkerSpecific, TestWorkerSpecificPlusIterator) {
  parlay::WorkerSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  auto it = list.begin();
  auto current = it;
  for (unsigned p = 0; p < parlay::num_workers(); p++) {
    auto next = (it + p);
    ASSERT_EQ(current, next);
    ++current;
  }
}

TEST(TestWorkerSpecific, TestWorkerSpecificMinusIterator) {
  parlay::WorkerSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  auto it = list.end();
  auto current = list.end();
  for (unsigned p = 1; p <= parlay::num_workers(); p++) {
    auto next = (it - p);
    --current;
    ASSERT_EQ(current, next);
  }
}


TEST(TestWorkerSpecific, TestWorkerSpecificIteratorDifference) {
  parlay::WorkerSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  auto it = list.begin();
  for (unsigned p = 0; p < parlay::num_workers(); p++) {
    for (unsigned p2 = 0; p + p2 < parlay::num_workers(); p2++) {
      auto first = (it + p);
      auto second = first + p2;
      auto diff = second - first;
      auto neg_diff = first - second;
      ASSERT_EQ(diff, p2);
      ASSERT_EQ(neg_diff, -static_cast<int>(p2));

      auto again = it + (p + p2);
      ASSERT_EQ(second, again);
    }
  }
}

TEST(TestWorkerSpecific, TestParallelIterate) {
  parlay::WorkerSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::worker_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  parlay::for_each(list, [](int x) {
    ASSERT_GE(x, 0);
    ASSERT_LT(x, parlay::num_workers());
  });
}

TEST(TestWorkerSpecific, TestLastElement) {
  parlay::WorkerSpecific<int> list([]() { return 42; });

  // Only touch the last element/chunk to make sure that the middle ones are also initialized
  parlay::parallel_for(0, 1000, [&](size_t) {
    if (parlay::worker_id() == parlay::num_workers() - 1) {
      *list = 42;
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  for (int x : list) {
    ASSERT_EQ(x, 42);
  }
}
