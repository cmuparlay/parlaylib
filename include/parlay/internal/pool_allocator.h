
#ifndef PARLAY_INTERNAL_POOL_ALLOCATOR_H
#define PARLAY_INTERNAL_POOL_ALLOCATOR_H

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "../parallel.h"
#include "../utilities.h"

#include "block_allocator.h"

#include "concurrency/hazptr_stack.h"

namespace parlay {
namespace internal {

// ****************************************
//    pool_allocator
// ****************************************

// Allocates headerless blocks from pools of different sizes.
// A vector of pool sizes is given to the constructor.
// Sizes must be at least 8, and must increase.
// For pools of small blocks (below large_threshold) each thread keeps a
// thread local list of elements from each pool using the block_allocator.
// For large blocks there is only one pool shared by all threads. For
// blocks larger than the maximum pool size, allocation and deallocation
// is performed directly by operator new.
struct pool_allocator {

  // Maximum alignment guaranteed by the allocator
  static inline constexpr size_t max_alignment = 128;
  
 private:
  static inline constexpr size_t large_threshold = (1 << 18);

  size_t num_buckets;
  size_t num_small;
  size_t max_small;
  size_t max_size;
  std::atomic<size_t> large_allocated{0};
  std::atomic<size_t> large_used{0};

  std::unique_ptr<size_t[]> sizes;
  std::unique_ptr<internal::hazptr_stack<void*>[]> large_buckets;
  unique_array<block_allocator> small_allocators;

  void* allocate_large(size_t n) {

    size_t bucket = num_small;
    size_t alloc_size;
    large_used += n;

    if (n <= max_size) {
      while (n > sizes[bucket]) bucket++;
      std::optional<void*> r = large_buckets[bucket-num_small].pop();
      if (r) return *r;
      alloc_size = sizes[bucket];
    } else alloc_size = n;

    // Alloc size must be a multiple of the alignment
    // Round up to the next multiple.
    if (alloc_size % max_alignment != 0) {
      alloc_size += (max_alignment - (alloc_size % max_alignment));
    }

    void* a = ::operator new(alloc_size, std::align_val_t{max_alignment});
    if (a == nullptr) throw std::bad_alloc();

    large_allocated += n;
    return a;
  }

  void deallocate_large(void* ptr, size_t n) {
    large_used -= n;
    if (n > max_size) {
      ::operator delete(ptr, std::align_val_t{max_alignment});
      large_allocated -= n;
    } else {
      size_t bucket = num_small;
      while (n > sizes[bucket]) bucket++;
      large_buckets[bucket-num_small].push(ptr);
    }
  }

 public:
  pool_allocator(const pool_allocator&) = delete;
  pool_allocator(pool_allocator&&) = delete;
  pool_allocator& operator=(const pool_allocator&) = delete;
  pool_allocator& operator=(pool_allocator&&) = delete;

  ~pool_allocator() {
    clear();
  }

  explicit pool_allocator(const std::vector<size_t>& sizes_) :
      num_buckets(sizes_.size()),
      sizes(std::make_unique<size_t[]>(num_buckets)) {

    std::copy(std::begin(sizes_), std::end(sizes_), sizes.get());
    max_size = sizes[num_buckets-1];
    num_small = 0;
    while (num_small < num_buckets && sizes[num_small] < large_threshold)
      num_small++;
    max_small = (num_small > 0) ? sizes[num_small - 1] : 0;

    large_buckets = std::make_unique<internal::hazptr_stack<void*>[]>(num_buckets-num_small);
    small_allocators = make_unique_array<internal::block_allocator>(num_small, [&](size_t i) {
      return internal::block_allocator(sizes[i], max_alignment);
    });

#ifndef NDEBUG
    size_t prev_bucket_size = 0;
    for (size_t i = 0; i < num_small; i++) {
      size_t bucket_size = sizes[i];
      assert(bucket_size >= 8);
      assert(bucket_size > prev_bucket_size);
      prev_bucket_size = bucket_size;
    }
#endif
  }

  void* allocate(size_t n) {
    if (n > max_small) return allocate_large(n);
    size_t bucket = 0;
    while (n > sizes[bucket]) bucket++;
    return small_allocators[bucket].alloc();
  }

  void deallocate(void* ptr, size_t n) {
    if (n > max_small) deallocate_large(ptr, n);
    else {
      size_t bucket = 0;
      while (n > sizes[bucket]) bucket++;
      small_allocators[bucket].free(ptr);
    }
  }

  // allocate, touch, and free to make sure space for small blocks is paged in
  void reserve(size_t bytes) {
    size_t bc = bytes/max_small;
    std::vector<void*> h(bc);
    parallel_for(0, bc, [&] (size_t i) {
      h[i] = allocate(max_small);
    }, 1);
    parallel_for(0, bc, [&] (size_t i) {
      for (size_t j=0; j < max_small; j += (1 << 12)) {
        static_cast<std::byte*>(h[i])[j] = std::byte{0};
      }
    }, 1);
    for (size_t i=0; i < bc; i++)
      deallocate(h[i], max_small);
  }

  void print_stats() {
    size_t total_a = 0;
    size_t total_u = 0;
    for (size_t i = 0; i < num_small; i++) {
      size_t bucket_size = sizes[i];
      size_t allocated = small_allocators[i].num_allocated_blocks();
      size_t used = small_allocators[i].num_used_blocks();
      total_a += allocated * bucket_size;
      total_u += used * bucket_size;
      std::cout << "size = " << bucket_size << ", allocated = " << allocated
                << ", used = " << used << std::endl;
    }
    std::cout << "Large allocated = " << large_allocated << std::endl;
    std::cout << "Total bytes allocated = " << total_a + large_allocated << std::endl;
    std::cout << "Total bytes used = " << total_u << std::endl;
  }

  // pair of total currently used space, and total unused space the allocator has in reserve
  std::pair<size_t,size_t> stats() {
    size_t total_a = large_allocated;
    size_t total_u = large_used;
    for (size_t i = 0; i < num_small; i++) {
      size_t bucket_size = sizes[i];
      size_t allocated = small_allocators[i].num_allocated_blocks();
      size_t used = small_allocators[i].num_used_blocks();
      total_a += allocated * bucket_size;
      total_u += used * bucket_size;
    }
    return {total_u, total_a-total_u};
  }

  void clear() {
    for (size_t i = num_small; i < num_buckets; i++) {
      std::optional<void*> r = large_buckets[i-num_small].pop();
      while (r) {
        large_allocated -= sizes[i];
        ::operator delete(*r, std::align_val_t{max_alignment});
        r = large_buckets[i-num_small].pop();
      }
    }
  }
};

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_POOL_ALLOCATOR_H
