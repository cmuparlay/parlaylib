// A concurrent allocator for any fixed type T
// Keeps a local pool per processor
// Grabs list_size elements from a global pool if empty, and
// Returns list_size elements to the global pool when local pool=2*list_size
// Keeps track of number of allocated elements.
// Probably more efficient than a general purpose allocator

#ifndef PARLAY_BLOCK_ALLOCATOR_H_
#define PARLAY_BLOCK_ALLOCATOR_H_

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <atomic>
#include <optional>

#include "concurrent_stack.h"
#include "memory_size.h"

#include "../utilities.h"

namespace parlay {

struct block_allocator {
private:

  static const size_t default_list_bytes = (1 << 22) - 64; // in bytes
  static const size_t pad_size = 256;

  struct block {
    block* next;
  };

  using block_p = block*;

  struct alignas(64) thread_list {
    size_t sz;
    block_p head;
    block_p mid;
    char cache_line[pad_size];
    thread_list() : sz(0), head(nullptr) {};
  };

  bool initialized{false};
  concurrent_stack<char*> pool_roots;
  concurrent_stack<block_p> global_stack;
  thread_list* local_lists;

  size_t list_length;
  size_t max_blocks;
  size_t block_size_;
  std::atomic<size_t> blocks_allocated;
  
  block_allocator(const block_allocator&) = delete;
  block_allocator(block_allocator&&) = delete;
  block_allocator& operator=(const block_allocator&) = delete;
  block_allocator& operator=(block_allocator&&) = delete;

public:
  const size_t thread_count;
  size_t block_size () {return block_size_;}
  size_t num_allocated_blocks() {return blocks_allocated.load();}

  // Allocate a new list of list_length elements

  auto initialize_list(block_p start) -> block_p {
    parallel_for (0, list_length - 1, [&] (size_t i) {
					block_p p =  reinterpret_cast<block_p>(reinterpret_cast<char*>(start) + i * block_size_);
					p->next = reinterpret_cast<block_p>(reinterpret_cast<char*>(p) + block_size_);
    }, 1000, true);
    block_p last =  reinterpret_cast<block_p>(reinterpret_cast<char*>(start) + (list_length-1) * block_size_);
    last->next = nullptr;
    return start;
  }

  size_t num_used_blocks() {
    size_t free_blocks = global_stack.size()*list_length;
    for (size_t i = 0; i < thread_count; ++i)
      free_blocks += local_lists[i].sz;
    return blocks_allocated.load() - free_blocks;
  }

  auto allocate_blocks(size_t num_blocks) -> char* {
    char* start = (char*) ::operator new(num_blocks * block_size_+ pad_size, std::align_val_t{pad_size});
    assert(start != nullptr);

    blocks_allocated.fetch_add(num_blocks);
    assert(blocks_allocated.load() <= max_blocks);

    pool_roots.push(start); // keep track so can free later
    return start;
  }

  // Either grab a list from the global pool, or if there is none
  // then allocate a new list
  auto get_list() -> block_p {
    std::optional<block_p> rem = global_stack.pop();
    if (rem) return *rem;
    block_p start = reinterpret_cast<block_p>(allocate_blocks(list_length));
    return initialize_list(start);
  }

  // Allocate n elements across however many lists are needed (rounded up)
  void reserve(size_t n) {
    size_t num_lists = thread_count + (n + list_length - 1) / list_length;
    char* start = allocate_blocks(list_length*num_lists);
    parallel_for(0, num_lists, [&] (size_t i) {
      block_p offset = reinterpret_cast<block_p>(start + i * list_length * block_size_);
      global_stack.push(initialize_list(offset));
   });
  }

  void print_stats() {
    size_t used = num_used_blocks();
    size_t allocated = num_allocated_blocks();
    size_t sz = block_size();
    std::cout << "Used: " << used << ", allocated: " << allocated
	      << ", block size: " << sz
	      << ", bytes: " << sz*allocated << std::endl;
  }

  explicit block_allocator(size_t block_size,
		  size_t reserved_blocks = 0, 
		  size_t list_length_ = 0, 
		  size_t max_blocks_ = 0) : thread_count(num_workers()) {
    blocks_allocated.store(0);
    block_size_ = block_size;
    if (list_length_ == 0)
      list_length = default_list_bytes / block_size;
    else list_length = list_length_ / block_size;
    if  (max_blocks_ == 0)
      max_blocks = (3*getMemorySize()/block_size)/4;
    else max_blocks = max_blocks_;

    reserve(reserved_blocks);

    // all local lists start out empty
    local_lists = new thread_list[thread_count];
    initialized = true;
  }

  bool clear() {
    if (num_used_blocks() > 0) 
      //std::cout << "Warning: not clearing memory pool, block_size=" << block_size()
      //   << " : " << num_used_blocks() << " allocated blocks remain" << std::endl;
      return true;
    else {
      // clear lists
      for (size_t i = 0; i < thread_count; ++i)
        local_lists[i].sz = 0;
  
      // throw away all allocated memory
      std::optional<char*> x;
      while ((x = pool_roots.pop())) ::operator delete(*x, std::align_val_t{pad_size});
      pool_roots.clear();
      global_stack.clear();
      blocks_allocated.store(0);
      return false;
    }
  }

  ~block_allocator() {
    clear();
    delete[] local_lists;
  }

  void free(void* ptr) {
    block_p new_node = reinterpret_cast<block_p>(ptr);
    size_t id = worker_id();

    if (local_lists[id].sz == list_length+1) {
      local_lists[id].mid = local_lists[id].head;
    } else if (local_lists[id].sz == 2*list_length) {
      global_stack.push(local_lists[id].mid->next);
      local_lists[id].mid->next = nullptr;
      local_lists[id].sz = list_length;
    }
    new_node->next = local_lists[id].head;
    local_lists[id].head = new_node;
    local_lists[id].sz++;
  }

  inline void* alloc() {
    size_t id = worker_id();

    if (local_lists[id].sz == 0)  {
      local_lists[id].head = get_list();
      local_lists[id].sz = list_length;
    }

    local_lists[id].sz--;
    block_p p = local_lists[id].head;
    local_lists[id].head = local_lists[id].head->next;

    return (void*) p;
  }

};

}  // namespace parlay

#endif  // PARLAY_BLOCK_ALLOCATOR_H_
