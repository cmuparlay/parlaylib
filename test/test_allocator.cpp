#include "gtest/gtest.h"

#include <utility>
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

TEST(TestAllocator, TestTypeAllocatorUninitialized) {
  using vector_allocator = parlay::type_allocator<std::vector<int>>;
  std::vector<int>* mem = vector_allocator::alloc();
  new (mem) std::vector<int>();
  auto& a = *mem;
  for (int i = 0; i < 1000000; i++) {
    a.push_back(i);
  }
  a.clear();
  for (int i = 0; i < 1000000; i++) {
    a.push_back(i);
  }
  a.~vector<int>();
  vector_allocator::free(mem);
}

TEST(TestAllocator, TestTypeAllocatorConstructed) {
  using vector_allocator = parlay::type_allocator<std::vector<int>>;
  std::vector<int>* mem = vector_allocator::create();
  auto& a = *mem;
  for (int i = 0; i < 1000000; i++) {
    a.push_back(i);
  }
  a.clear();
  for (int i = 0; i < 1000000; i++) {
    a.push_back(i);
  }
  vector_allocator::destroy(mem);
}

// Checks that type_allocator allocates small blocks successfully.
TEST(TestAllocator, TestTypeAllocatorForSmallSizes) {
  using char_allocator = parlay::type_allocator<char>;
  char* a = char_allocator::alloc();
  new (a) char{'A'};
  char* b = char_allocator::alloc();
  new (b) char{'B'};
  char_allocator::free(a);
  char_allocator::free(b);
}

template<typename F, size_t... Is>
void for_each_alignment(F&& f, std::index_sequence<Is...>) {
  ( f(std::integral_constant<size_t, size_t{1} << Is>{}), ... );
}

TEST(TestAllocator, TestParlayAllocatorOverAligned) {
  auto test_align = [&](auto alignment_constant) {
    constexpr size_t Align = alignment_constant.value;
    struct alignas(Align) OverAlignedStruct { char x; };
    auto a = parlay::allocator<OverAlignedStruct>{};
    OverAlignedStruct* p = a.allocate(1);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % Align, 0);
    a.deallocate(p, 1);
  };

  // Test on alignments 2^{0,1,2,3,...,13}
  // 2^13 is a common maximum alignment allowed by implementations
  auto alignments = std::make_index_sequence<14>();
  for_each_alignment(test_align, alignments);
}

TEST(TestAllocator, TestTypeAllocatorAlignment) {

  auto test_align = [&](auto alignment_constant) {
    constexpr size_t Align = alignment_constant.value;
    struct alignas(Align) OverAlignedStruct { char x; };
    using my_allocator = parlay::type_allocator<OverAlignedStruct>;
    OverAlignedStruct* p = my_allocator::alloc();
    ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % Align, 0);
    my_allocator::free(p);
  };

  // Test on alignments 2^{0,1,2,3,...,13}
  // 2^13 is a common maximum alignment allowed by implementations
  auto alignments = std::make_index_sequence<14>();
  for_each_alignment(test_align, alignments);
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
      ASSERT_NE(p, nullptr);
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

// Allocate blocks of memory of random sizes and random alignments
// and fill them with characters, then free the blocks.
TEST(TestAllocator, TestAlignedPMallocAndPFree) {
  parlay::random rng(0);
  std::vector<void*> memory;
  for (int j = 1; j < 1000000; j *= 10) {
    for (int i = 0; i < 10000000 / j; i++) {
      size_t size = j * (rng.ith_rand(j + i) % 9 + 1);
      size_t log_align = (rng.ith_rand(100000 + i + j)) % (1 + parlay::log2_up(size));
      size_t alignment = size_t{1} << log_align;
      void* p = parlay::p_malloc(size, alignment);
      ASSERT_NE(p, nullptr);
      ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0);
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

  using Xallocator = parlay::type_allocator<X>;

  X* x = Xallocator::alloc();
  ASSERT_NE(x, nullptr);
  Xallocator::free(x);
}


parlay::sequence<parlay::sequence<int>> a;

TEST(TestAllocator, TestStaticGlobal) {
  parlay::sequence<parlay::sequence<int>> b{{2, 3, 4, 5}, {1, 2, 3, 4}};
  a = b;
}
