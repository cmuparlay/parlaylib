#include "gtest/gtest.h"

#include <vector>

#include <parlay/alloc.h>
#include <parlay/random.h>

TEST(TestAllocator, TestParlayAllocator) {
  std::vector<int, parlay::allocator<int>> a;
  a.reserve(100000);
  for (int i = 0; i < 100000; i++) {
    a.push_back(i);
  }
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(a[i], i);
  }
  a.clear();
  for (int i = 0; i < 100000; i++) {
    a.push_back(i);
  }
  for (int i = 0; i < 100000; i++) {
    ASSERT_EQ(a[i], i);
  }
}

TEST(TestAllocator, TestTypeAllocator) {
  using vector_allocator = parlay::type_allocator<std::vector<int>>;
  std::vector<int>* mem = vector_allocator::alloc();
  new (mem) std::vector<int>(100000);
  auto& a = *mem;
  a.reserve(100000);
  for (int i = 0; i < 100000; i++) {
    a.push_back(i);
  }
  a.clear();
  for (int i = 0; i < 100000; i++) {
    a.push_back(i);
  }
  a.~vector<int>();
  vector_allocator::free(mem);
}

struct alignas(256) StrangeAlignedStruct {
  int x;
};

TEST(TestAllocator, TestTypeAllocatorAlignment) {
  using strange_allocator = parlay::type_allocator<StrangeAlignedStruct>;
  
  StrangeAlignedStruct* s = strange_allocator::alloc();
  s->x = 5;
  ASSERT_EQ(s->x, 5);
  strange_allocator::free(s);
}

// Allocate blocks of memory of random sizes and fill
// them with characters, then free the blocks.
TEST(TestAllocator, TestPMallocAndPFree) {
  parlay::random rng(0);
  std::vector<void*> memory;
  for (int j = 1; j < 1000000; j *= 10) {
    for (int i = 0; i < 10000000 / j; i++) {
      size_t size = j * (rng.ith_rand(j + i) % 9 + 1);
      void* p = parlay::p_malloc(size);
      memory.push_back(p);
      for (size_t b = 0; b < size; b++) {
        new (static_cast<char*>(p) + b) char('b');
      }
    }
  }
  for (void* p : memory) {
    parlay::p_free(p);
  }
}
