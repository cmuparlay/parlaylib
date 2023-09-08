#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <parlay/slice.h>
#include <parlay/thread_specific.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>


TEST(TestThreadSpecific, TestUniqueIds) {
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

TEST(TestThreadSpecific, TestThreadSpecific) {

  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000000, [&](size_t) {
    *list += 1;
  }, 1);

  int total = 0;
  list.for_each([&](int& x) { total += x; });

  ASSERT_EQ(total, 1000000);
}

TEST(TestThreadSpecific, TestThreadSpecificCustomConstructor) {

  parlay::ThreadSpecific<int> list([]() { return 42; });

  parlay::parallel_for(0, 1000000, [&](size_t) {
    ASSERT_EQ(*list, 42);
  }, 1);

}

TEST(TestThreadSpecific, TestThreadSpecificCustomConstructorParam) {

  parlay::ThreadSpecific<int> list([](parlay::thread_id_type tid) { return tid; });

  parlay::parallel_for(0, 1000000, [&](size_t) {
    ASSERT_EQ(*list, parlay::my_thread_id());
  }, 1);

}

TEST(TestThreadSpecific, TestThreadSpecificDestructor) {

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

    parlay::ThreadSpecific<MyType> list([&]() { return MyType{constructions, destructions}; });

    parlay::parallel_for(0, 1000000, [&](size_t) {
      ASSERT_EQ(list->destructions.load(), 0);
    }, 1);
  }

  ASSERT_GE(constructions.load(), parlay::num_thread_ids());
  ASSERT_EQ(constructions.load(), destructions.load());
}

TEST(TestThreadSpecific, TestThreadSpecificUnique) {

  // Make sure the atomic<bool>s are initialized to false.
  parlay::ThreadSpecific<std::atomic<bool>> list([]() { return std::atomic<bool>{false}; });

  parlay::parallel_for(0, 100000, [&](size_t) {
    ASSERT_FALSE(list->exchange(true));
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    ASSERT_TRUE(list->exchange(false));
  });
}

TEST(TestThreadSpecific, TestThreadSpecificConst) {
  parlay::ThreadSpecific<int> list;

  *list = 42;
  const auto& clist = list;
  ASSERT_EQ(*clist, 42);
}

TEST(TestThreadSpecific, TestThreadSpecificIterate) {
  parlay::ThreadSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  int tid = 0;

  for (int x : list) {
    ASSERT_TRUE((x == tid++) || (x == -1));
  }
}

TEST(TestThreadSpecific, TestThreadSpecificConstIterate) {
  parlay::ThreadSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  int tid = 0;


  const auto& clist = list;

  for (int x : clist) {
    ASSERT_TRUE((x == tid++) || (x == -1));
  }
}

TEST(TestThreadSpecific, TestThreadSpecificIterateReverse) {
  parlay::ThreadSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  int tid = (int)parlay::num_thread_ids();

  auto rev = parlay::make_slice(std::make_reverse_iterator(list.end()), std::make_reverse_iterator(list.begin()));

  for (int x : rev) {
    ASSERT_TRUE((x == --tid) || (x == -1));
  }
}

TEST(TestThreadSpecific, TestThreadSpecificIterateInitialize) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::my_thread_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  // Ensure that the list values are initialized
  for (int x : list) {
    ASSERT_EQ(x, 42);
  }
}

TEST(TestThreadSpecific, TestThreadSpecificIterateReverseInitialize) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::my_thread_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto rev = parlay::make_slice(std::make_reverse_iterator(list.end()), std::make_reverse_iterator(list.begin()));

  for (int x : rev) {
    ASSERT_EQ(x, 42);
  }
}

TEST(TestThreadSpecific, TestThreadSpecificRandomAccessIterator) {
  parlay::ThreadSpecific<int> list([]() { return -1; });

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.begin();

  for (unsigned p = 0; p < parlay::num_thread_ids(); p++) {
    auto val = it[p];
    ASSERT_TRUE((val == (int)p) || (val == -1));
  }
}

TEST(TestThreadSpecific, TestThreadSpecificPlusIterator) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.begin();
  auto current = it;
  for (unsigned p = 0; p < parlay::num_thread_ids(); p++) {
    auto next = (it + p);
    ASSERT_EQ(current, next);
    ++current;
  }
}

TEST(TestThreadSpecific, TestThreadSpecificMinusIterator) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.end();
  auto current = list.end();
  for (unsigned p = 1; p <= parlay::num_thread_ids(); p++) {
    auto next = (it - p);
    --current;
    ASSERT_EQ(current, next);
  }
}

TEST(TestThreadSpecific, TestThreadSpecificPlusIteratorInitialize) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::my_thread_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.begin();
  for (unsigned p = 0; p < parlay::num_thread_ids(); p++) {
    auto val = *(it + p);
    ASSERT_EQ(val, 42);
  }
}

TEST(TestThreadSpecific, TestThreadSpecificMinusIteratorInitialize) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::my_thread_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.end();
  for (unsigned p = parlay::num_thread_ids(); p > 0; p--) {
    auto val = *(it - p);
    ASSERT_EQ(val, 42);
  }
}

TEST(TestThreadSpecific, TestThreadSpecificIteratorDifference) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_EQ(parlay::size(list), parlay::num_thread_ids());

  auto it = list.begin();
  for (unsigned p = 0; p < parlay::num_thread_ids(); p++) {
    for (unsigned p2 = 0; p + p2 < parlay::num_thread_ids(); p2++) {
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

TEST(TestThreadSpecific, TestThreadSpecificRandomAccessIteratorInitialize) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Ensure that each thread has an ID assigned without actually touching the list
  parlay::parallel_for(0, 1000, [&](size_t) {
    parlay::my_thread_id();
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::num_thread_ids(), parlay::num_workers());

  auto it = list.begin();
  for (unsigned p = 0; p < parlay::num_thread_ids(); p++) {
    auto val = it[p];
    ASSERT_EQ(val, 42);
  }
}

TEST(TestThreadSpecific, TestParallelIterate) {
  parlay::ThreadSpecific<int> list;

  parlay::parallel_for(0, 1000, [&](size_t) {
    *list = static_cast<int>(parlay::my_thread_id());
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  ASSERT_LE(parlay::size(list), parlay::num_thread_ids());

  parlay::for_each(list, [](int x) {
    ASSERT_GE(x, 0);
    ASSERT_LT(x, parlay::num_thread_ids());
  });
}

TEST(TestThreadSpecific, TestLastElement) {
  parlay::ThreadSpecific<int> list([]() { return 42; });

  // Only touch the last element/chunk to make sure that the middle ones are also initialized
  parlay::parallel_for(0, 1000, [&](size_t) {
    if (parlay::my_thread_id() == parlay::num_thread_ids() - 1) {
      *list = 42;
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }, 1);

  for (int x : list) {
    ASSERT_EQ(x, 42);
  }
}

#if defined(PARLAY_USING_PARLAY_SCHEDULER)

TEST(TestThreadSpecific, TestMultipleSchedulers) {
  parlay::ThreadSpecific<std::atomic<bool>> list([]() { return std::atomic<bool>{false}; });

  // Every thread checks that no one else is accidentally sharing a slot with them
  auto job = [&]() {
    parlay::parallel_for(0, 100000, [&](size_t) {
      ASSERT_FALSE(list->exchange(true));
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      ASSERT_TRUE(list->exchange(false));
    });
  };

  parlay::parallel_do(
    [&]() { parlay::execute_with_scheduler(2*std::thread::hardware_concurrency(), job); },
    [&]() { parlay::execute_with_scheduler(2*std::thread::hardware_concurrency(), job); }
  );
}

#endif  // defined(PARLAY_USING_PARLAY_SCHEDULER)
