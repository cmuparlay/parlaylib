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


// Checks that type_allocator allocates small blocks successfully.
TEST(TestAllocator, TestTypeAllocatorForSmallSizes) {
  using char_allocator = parlay::type_allocator<char>;
  char* a = char_allocator::alloc();
  *a = 'A';
  char* b = char_allocator::alloc();
  *b = 'B';
  char_allocator::free(a);
  char_allocator::free(b);
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


TEST(TestAllocator, TestTypeAllocatorLarge) {
  // Larger than block_allocators default size
  struct X { char x[1<<19]; };

  auto a = parlay::type_allocator<X>();

  X* x = a.alloc();
  ASSERT_NE(x, nullptr);
  a.free(x);
}

TEST(TestAllocator, TestTypeAllocatorOverAligned) {
  // On my machine, all the block allocator pools were being
  // allocated on 2046-byte alignments, so 4096 was the
  // smallest alignment that would actually test whether
  // the allocator was respecting it. Of course there's
  // a 50-50 chance that it gets lucky...
  struct alignas(4096) X {
    char x;
  };

  auto a = parlay::type_allocator<X>();
  X* x = a.alloc();
  ASSERT_NE(x, nullptr);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(x) % 4096, 0);  // check alignment explicitly for good measure
  x->x = 'b';                                           // if not aligned, UBSAN should catch the unaligned access
  a.free(x);
}

parlay::sequence<parlay::sequence<int>> a;

TEST(TestAllocator, TestStaticGlobal) {
  parlay::sequence<parlay::sequence<int>> b{{2, 3, 4, 5}, {1, 2, 3, 4}};
  a = b;
}
