#ifndef PARLAY_RELOCATION_H_
#define PARLAY_RELOCATION_H_

#include <cstring>

#include <algorithm>
#include <iterator>
#include <memory>
#include <new>                 // IWYU pragma: keep
#include <type_traits>
#include <utility>

#include "parallel.h"
#include "range.h"             // IWYU pragma: keep
#include "type_traits.h"       // IWYU pragma: keep
#include "utilities.h"         // IWYU pragma: keep

#include "internal/debug_uninitialized.h"

namespace parlay {

/*
    Relocation (a.k.a. "destructive move")

  The relocation of object a into memory b is equivalent to a move
  construction of a into b, followed by the destruction of what
  remains at a. In other words, it is

    new (b) T(std::move(*a));
    a->~T();

  For many types, however, this can be optimized by replacing it with just

    std::memcpy(b, a, sizeof(T));

   We call any such types trivially relocatable. This is clearly
   true for any trivial type, but it also turns out to be true
   for most other standard types, such as vectors, unique_ptrs,
   and more. The key observation is that the only reason that
   the move operations of these types is non-trivial is because
   they must clear out the source object after moving from it.
   If, however, the source object is treated as uninitialized
   memory after relocation, then it does not have to be cleared
   out, since its destructor will not run.

   A strong motivating use case for relocation is in dynamically
   sized containers (e.g. vector, or parlay::sequence). When
   performing a resize operation, one has to move all of the
   contents of the old buffer into the new one, and destroy
   the contents of the old buffer, like so (ignoring allocator
   details)

   parallel_for(0, n, [&](size_t i) {
     new (&new_buffer[i]) std::move(current_buffer[i]));
     current_buffer[i].~value_type();
   });

   This can be replaced with

   parallel_for(0, n, [&](size_t i) {
     uninitialized_relocate(&new_buffer[i], &current_buffer[i]);
   });

   or even, for better performance yet

   uninitialized_relocate_n(new_buffer, current_buffer, n);

   The uninitialized_relocate functions will use the optimized
   memcpy-based approach for any types for which it is suitable,
   and otherwise, will fall back to the generic approach.
*/

// Relocate the given range of n elements [from, from + n) into uninitialized
// memory at [to, to + n), such that both the source and destination memory
// were allocated by the given allocator.
template<typename It1, typename It2, typename Alloc>
inline void uninitialized_relocate_n_a(It1 to, It2 from, size_t n, Alloc& alloc) {
  using T = typename std::iterator_traits<It1>::value_type;
  static_assert(std::is_same_v<typename std::iterator_traits<It2>::value_type, T>);
  static_assert(std::is_same_v<typename std::allocator_traits<Alloc>::value_type, T>);

  constexpr bool trivially_relocatable = is_trivially_relocatable_v<T>;
  constexpr bool trivial_alloc = is_trivial_allocator_v<Alloc, T>;
  constexpr bool contiguous = is_contiguous_iterator_v<It1> && is_contiguous_iterator_v<It2>;
  constexpr bool random_access = is_random_access_iterator_v<It1> && is_random_access_iterator_v<It2>;

  // The most efficient scenario -- The objects are trivially relocatable, the allocator
  // has no special behaviour, and the iterators point to contiguous memory so we can
  // memcpy chunks of more than one T object at a time.
  if constexpr (trivially_relocatable && contiguous && trivial_alloc) {
    constexpr size_t chunk_size = 1024 * sizeof(size_t) / sizeof(T);
    const size_t n_chunks = (n + chunk_size - 1) / chunk_size;
    parallel_for(0, n_chunks, [&](size_t i) {
      size_t n_objects = (std::min)(chunk_size, n - i * chunk_size);
      size_t n_bytes = sizeof(T) * n_objects;
      void* src = static_cast<void*>(std::addressof(*(from + i * chunk_size)));
      void* dest = static_cast<void*>(std::addressof(*(to + i * chunk_size)));
      std::memcpy(dest, src, n_bytes);
    }, 1);
  // The next best thing -- If the objects are trivially relocatable and the allocator
  // has no special behaviour, so long as the iterators are random access, we can still
  // relocate everything in parallel, just not by memcpying multiple objects at a time
  } else if constexpr (trivially_relocatable && random_access && trivial_alloc) {
    constexpr size_t chunk_size = 1024 * sizeof(size_t) / sizeof(T);
    const size_t n_chunks = (n + chunk_size - 1) / chunk_size;
    parallel_for(0, n_chunks, [&](size_t i) {
      for (size_t j = 0; j < chunk_size && (j + i *chunk_size < n); j++) {
        void* src = static_cast<void*>(std::addressof(from[j + i * chunk_size]));
        void* dest = static_cast<void*>(std::addressof(to[j + i * chunk_size]));
        std::memcpy(dest, src, sizeof(T));
      }
    }, 1);
  }
  // The iterators are not random access, but we can still relocate, just not in parallel
  else if constexpr (trivially_relocatable && trivial_alloc) {
    for (size_t i = 0; i < n; i++) {
      std::memcpy(std::addressof(*to), std::addressof(*from), sizeof(T));
      to++;
      from++;
    }
  }
  // After this point, the object can not be trivially relocated, either because it is not
  // trivially relocatable, or because the allocator has specialized behaviour. We now fall
  // back to just doing a "destructive move" manually.
  else if constexpr (random_access) {
    static_assert(std::is_move_constructible<T>::value);
    static_assert(std::is_destructible<T>::value);
    parallel_for(0, n, [&](size_t i) {
      PARLAY_ASSERT_UNINITIALIZED(to[i]);
      std::allocator_traits<Alloc>::construct(alloc, std::addressof(to[i]), std::move(from[i]));
      std::allocator_traits<Alloc>::destroy(alloc, std::addressof(from[i]));
      PARLAY_ASSERT_UNINITIALIZED(from[i]);
    });
  }
  // The worst case. No parallelism and no fast relocation.
  else {
    static_assert(std::is_move_constructible<T>::value);
    static_assert(std::is_destructible<T>::value);
    for (size_t i = 0; i < n; i++) {
      PARLAY_ASSERT_UNINITIALIZED(*to);
      std::allocator_traits<Alloc>::construct(alloc, std::addressof(*to), std::move(*from));
      std::allocator_traits<Alloc>::destroy(alloc, std::addressof(*from));
      PARLAY_ASSERT_UNINITIALIZED(*from);
      to++;
      from++;
    }
  }
}

// Relocate the given range of n elements [from, from + n) into uninitialized
// memory at [to, to + n). Relocation is done as if the memory was allocated
// by a standard allocator (e.g. std::allocator, parlay::allocator, malloc)
//
// For an allocator-aware version that respects the construction and destruction
// behaviour of the allocator, use uninitialized_relocate_n_a.
template<typename Iterator1, typename Iterator2>
inline void uninitialized_relocate_n(Iterator1 to, Iterator2 from, size_t n) {
  using T = typename std::iterator_traits<Iterator1>::value_type;
  std::allocator<T> a;
  uninitialized_relocate_n_a(to, from, n, a);
}

}

#endif  // PARLAY_RELOCATION_H_
