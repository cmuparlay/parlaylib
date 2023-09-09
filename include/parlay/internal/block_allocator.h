// A concurrent allocator for blocks of a fixed size
//
// Keeps a local pool per thread, and grabs list_length elements from a
// global pool if empty, and returns list_length elements to the global
// pool when local pool=2*list_length.
//
// Keeps track of number of allocated elements. Much more efficient
// than a general purpose allocator.
//
// Not generally intended for users. Users should use "type_allocator"
// which is a convenient wrapper around block_allocator that helps
// to manage memory for a specific type

#ifndef PARLAY_INTERNAL_BLOCK_ALLOCATOR_H_
#define PARLAY_INTERNAL_BLOCK_ALLOCATOR_H_

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <new>
#include <optional>

#include "../utilities.h"
#include "../thread_specific.h"

#include "concurrency/hazptr_stack.h"

#include "memory_size.h"

// IWYU pragma: no_include <array>
// IWYU pragma: no_include <vector>

namespace parlay {
namespace internal {

struct block_allocator {
 private:

  static inline constexpr size_t default_list_bytes = (1 << 18) - 64;  // in bytes
  static inline constexpr size_t min_alignment = 128;  // for cache line padding

  struct block {
    block* next;
  };

  struct alignas(128) local_list {
    size_t sz;
    block* head;
    block* mid;
    local_list() : sz(0), head(nullptr), mid(nullptr) {};
  };

  hazptr_stack<std::byte*> allocated_buffers;
  hazptr_stack<block*> global_stack;
  ThreadSpecific<local_list> my_local_list;

  size_t block_size;
  std::align_val_t block_align;
  size_t list_length;
  size_t max_blocks;
  std::atomic<size_t> blocks_allocated;

  block* get_block(std::byte* buffer, size_t i) const {
    // Since block is an aggregate type, it has implicit lifetime, so the following code
    // is defined behaviour in C++20 even if we haven't yet called a constructor of block.
    // It is still UB prior to C++20, but this is hard to avoid without losing performance.
    return from_bytes<block>(buffer + i * block_size);
  }

 public:
  block_allocator(const block_allocator&) = delete;
  block_allocator(block_allocator&&) = delete;
  block_allocator& operator=(const block_allocator&) = delete;
  block_allocator& operator=(block_allocator&&) = delete;

  size_t get_block_size() const { return block_size; }
  size_t num_allocated_blocks() const { return blocks_allocated.load(); }

  // Allocate a new list of list_length elements

  auto initialize_list(std::byte* buffer) const -> block* {
    for (size_t i=0; i < list_length - 1; i++) {
      new (buffer + i * block_size) block{get_block(buffer, i+1)};
    }
    new (buffer + (list_length - 1) * block_size) block{nullptr};
    return get_block(buffer, 0);
  }

  size_t num_used_blocks() {
    size_t free_blocks = global_stack.size()*list_length;
    my_local_list.for_each([&](auto&& list) {
      free_blocks += list.sz;
    });
    return blocks_allocated.load() - free_blocks;
  }

  auto allocate_blocks(size_t num_blocks) -> std::byte* {
    auto buffer = static_cast<std::byte*>(::operator new(num_blocks * block_size, block_align));
    assert(buffer != nullptr);

    blocks_allocated.fetch_add(num_blocks);
    assert(blocks_allocated.load() <= max_blocks);

    allocated_buffers.push(buffer); // keep track so can free later
    return buffer;
  }

  // Either grab a list from the global pool, or if there is none
  // then allocate a new list
  auto get_list() -> block* {
    std::optional<block*> rem = global_stack.pop();
    if (rem) return *rem;
    std::byte* buffer = allocate_blocks(list_length);
    return initialize_list(buffer);
  }

  // Allocate n elements across however many lists are needed (rounded up)
  [[deprecated]] void reserve(size_t) { }

  void print_stats() {
    size_t used = num_used_blocks();
    size_t allocated = num_allocated_blocks();
    size_t sz = get_block_size();
    std::cout << "Used: " << used << ", allocated: " << allocated
	      << ", block size: " << sz
	      << ", bytes: " << sz*allocated << "\n";
  }

  explicit block_allocator(size_t block_size_,
    size_t block_align_ = alignof(std::max_align_t),
    [[maybe_unused]] size_t reserved_blocks = 0,
    size_t list_length_ = 0,
    size_t max_blocks_ = 0) :
      my_local_list(),                                                               // Each block needs to be at least
      block_size(std::max<size_t>(block_size_, sizeof(block))),    // <------------- // large enough to hold the struct
      block_align(std::align_val_t{std::max<size_t>(block_align_, min_alignment)}),  // representing a free block.
      list_length(list_length_ == 0 ? (default_list_bytes + block_size + 1) / block_size : list_length_),
      max_blocks(max_blocks_ == 0 ? (3 * getMemorySize() / block_size) / 4 : max_blocks_),
      blocks_allocated(0) {

  }

  // Clears all memory ever allocated by this allocator. All allocated blocks
  // must be returned before calling this function.
  //
  // This operation is not safe to perform concurrently with any other operation
  //
  // Returns false if there exists blocks that haven't been returned, in which case
  // the operation fails and nothing is cleared. Returns true if no lingering
  // blocks were not freed, in which case the operation is successful
  bool clear() {
    if (num_used_blocks() > 0) {
      return false;
    }
    else {
      // clear lists
      my_local_list.for_each([](auto&& list) {
        list.sz = 0;
      });

      // throw away all allocated memory
      std::optional<std::byte*> x;
      while ((x = allocated_buffers.pop())) ::operator delete(*x, block_align);
      global_stack.clear();
      blocks_allocated.store(0);
      return true;
    }
  }

  ~block_allocator() {
    [[maybe_unused]] auto cleared = clear();
#if !defined(NDEBUG) && !defined(PARLAY_ALLOC_ALLOW_LEAK)
    if (!cleared) {
      std::cerr << "There are un-freed blocks obtained from block_allocator. If this is intentional you may"
                        "suppress this messsage with -DPARLAY_ALLOC_ALLOW_LEAK\n";
    }
#endif
  }

  void free(void* ptr) {

    if (my_local_list->sz == list_length+1) {
      my_local_list->mid = my_local_list->head;
    } else if (my_local_list->sz == 2*list_length) {
      global_stack.push(my_local_list->mid->next);
      my_local_list->mid->next = nullptr;
      my_local_list->sz = list_length;
    }

    auto new_node = new (ptr) block{my_local_list->head};
    my_local_list->head = new_node;
    my_local_list->sz++;
  }

  inline void* alloc() {
    if (my_local_list->sz == 0)  {
      auto new_list = get_list();

      // !! Critical problem !! If this task got stolen during get_list(), the
      // running thread may have changed, so we can't assume we are looking at
      // the same local list, so we have to double-check the (possibly different)
      // local list of the (possibly changed) thread

      if (my_local_list->sz == 0) {
        my_local_list->head = new_list;
        my_local_list->sz = list_length;
      }
      else {
        // Looks like the task got stolen and the new thread already had a
        // non-empty local list, so we can push the new one into the global
        // pool for someone else to use in the future
        global_stack.push(new_list);
      }
    }

    block* p = my_local_list->head;
    my_local_list->head = my_local_list->head->next;
    my_local_list->sz--;

    // Note: block is trivial, so it is legal to not call its destructor
    // before returning it and allowing its storage to be reused
    return static_cast<void*>(p);
  }

};

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_BLOCK_ALLOCATOR_H_
