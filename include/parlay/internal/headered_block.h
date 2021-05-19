#ifndef PARLAY_INTERNAL_HEADERED_BLOCK_H
#define PARLAY_INTERNAL_HEADERED_BLOCK_H

#include <cstddef>

#include <memory>

namespace parlay {
namespace internal {

// A block of raw memory with a header prepended to the front. Useful for
// tracking the size of an allocated block for custom allocator functions,
// or for tracking the capacity of a dynamically resizable buffer.
//
// Template arguments:
//  - header_type: The type of the header
//  - block_align (optional): The requested alignment of the block. Note
//    that the final alignment will only be as good as the alignment
//    provided by the underlying allocator.
//
// Usage:
//  - You must use the static factory method create(sz, header_, a),
//    which takes the size of the block in bytes (not including the
//    header), the initial contents of the header, and a reference
//    to an allocator a from which to allocate the buffer
//
//    [Example : Allocating a buffer for a container of n elements of type T]
//
//      using capacitated_block = headered_block<const size_t, alignof(T)>;
//
//      auto a = parlay::allocator<T>();
//      auto hb = capacitated_block::create(n * sizeof(T), n, a);
//
//      size_t capacity = hb->get_header();
//      T* buffer = static_cast<T*>(hb->get_block());
//
//      capacitated_block::destroy(hb, capacity, a);
//
template<typename header_type, size_t block_align = alignof(std::max_align_t)>
struct headered_block {
 private:
  std::byte* buffer_start;
  header_type header;
  union {
    alignas(block_align) std::byte block[1];
  };

  headered_block(std::byte* bytes, header_type &&header_) : buffer_start(bytes), header(std::move(header_)) {}

  ~headered_block() = default;

 public:
  headered_block() = delete;

  headered_block(const headered_block &) = delete;

  headered_block &operator=(const headered_block &) = delete;

  headered_block(headered_block &&) = delete;

  headered_block &operator=(headered_block &&) = delete;

  // Return a reference to the header of the block
  header_type &get_header() { return header; }

  // Return a reference to the header of the block
  const header_type &get_header() const { return header; }

  // Return a pointer to the block of raw memory
  std::byte *get_block() { return std::addressof(block); }

  // Return a pointer to the block of raw memory
  const std::byte *get_block() const { return std::addressof(block); }

  // Create a headered block of sz bytes (not including the header), prepended
  // with the given header, allocated with the given allocator.
  template<typename Alloc>
  static headered_block<header_type, block_align> *create(size_t sz, header_type header_, Alloc &a) {
    auto block_size = offsetof(headered_block, data) + sz;
    auto alloc_size = block_size + block_align - 1;
    auto byte_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<std::byte>(a);
    std::byte *bytes = std::allocator_traits<Alloc>::allocate(byte_allocator, alloc_size);
    void* aligned_ptr = static_cast<void*>(bytes);
    std::align(block_align, block_size, aligned_ptr, alloc_size);
    return new(aligned_ptr) headered_block(bytes, std::move(header_));
  }

  // Destroy the given headered block of sz bytes (not including the header)
  template<typename Alloc>
  static void destroy(headered_block<header_type, block_align> *b, size_t sz, Alloc &a) {
    b->~headered_block<header_type, block_align>();                 // Destroy the header
    auto block_size = offsetof(headered_block, data) + sz;
    auto alloc_size = block_size + block_align - 1;
    std::byte *bytes = b->buffer_start;
    auto byte_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<std::byte>(a);
    std::allocator_traits<Alloc>::deallocate(byte_allocator, bytes, alloc_size);
  }
};



}  // namespace internal
}  // namespace parlay

#endif //PARLAY_INTERNAL_HEADERED_BLOCK_H
