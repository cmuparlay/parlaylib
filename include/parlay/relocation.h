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
   Range-based Relocation

   A strong motivating use case for relocation is in dynamically
   sized containers (e.g. vector, or parlay::sequence). When
   performing a resize operation, one has to move the contents
   of the old buffer into the new one, and destroy the contents
   of the old buffer, like so (ignoring allocator details)

     parallel_for(0, n, [&](size_t i) {
       ::new (voidify(new_buffer[i])) std::move(current_buffer[i]));
       std::destroy_at(std::addressof(current_buffer[i]));
     });

   If current_buffer and new_buffer contain the same type (should
   always be true for a sequence container resize operation), then
   this can be replaced with

     parallel_for(0, n, [&](size_t i) {
       relocate_at(std::addressof(current_buffer[i]), std::addressof(new_buffer[i]));
     });

   However, it may be even more efficient to move chunks of objects in parallel,
   so for better performance, you can write

    uninitialized_relocate_n(current_buffer, n, new_buffer);

   The uninitialized_relocate functions will use the optimized
   memcpy-based approach for any types for which it is suitable,
   and otherwise, will fall back to the generic approach.
*/

// Relocate from source to dest, which may be of different
// (but compatible) types, which is equivalent to
//
//   ::new (voidify(dest)) T(std::move(source));
//   std::destroy_at(std::addressof(source));
//
template<typename T, typename U>
inline void relocate_or_move_and_destroy(T& source, U& dest) {
  static_assert(std::is_constructible_v<U, std::add_rvalue_reference_t<T>>);
  static_assert(std::is_destructible_v<T>);

  if constexpr (std::is_same_v<T, U>) {
    relocate_at(std::addressof(source), std::addressof(dest));
  }
  else {
    PARLAY_ASSERT_UNINITIALIZED(dest);
    ::new (voidify(dest)) T(std::move(source));
    std::destroy_at(std::addressof(source));
    PARLAY_ASSERT_UNINITIALIZED(source);
  }
}

// Relocate the given range of n elements [first, first + n) into uninitialized
// memory at [result, result + n).
template<typename InputIterator, typename Size, typename NoThrowForwardIterator>
inline std::pair<InputIterator, NoThrowForwardIterator> uninitialized_relocate_n(
    InputIterator first, Size n, NoThrowForwardIterator result) {

  static_assert(is_input_iterator_v<InputIterator>);
  static_assert(is_forward_iterator_v<NoThrowForwardIterator>);

  using T = iterator_value_type_t<NoThrowForwardIterator>;

  constexpr bool trivially_relocatable = is_trivially_relocatable_v<T> &&
                                         std::is_same_v<iterator_value_type_t<InputIterator>, T>;
  constexpr bool contiguous = is_contiguous_iterator_v<InputIterator> &&
                              is_contiguous_iterator_v<NoThrowForwardIterator>;
  constexpr bool random_access = is_random_access_iterator_v<InputIterator> &&
                                 is_random_access_iterator_v<NoThrowForwardIterator>;

  // The most efficient scenario -- The objects are trivially relocatable and the iterators
  // point to contiguous memory so, we can memcpy chunks of more than one T object at a time.
  if constexpr (contiguous && trivially_relocatable) {
    constexpr size_t chunk_size = 1024 * sizeof(size_t) / sizeof(T);
    const size_t n_chunks = (n + chunk_size - 1) / chunk_size;
    parallel_for(0, n_chunks, [&](size_t i) {
      size_t n_objects = (std::min)(chunk_size, n - i * chunk_size);
#if defined(__cpp_lib_trivially_relocatable)
      std::uninitialized_relocate_n(first + i * chunk_size, n_objects, result + i * chunk_size);
#else
      size_t n_bytes = sizeof(T) * n_objects;
      void* src = voidify(*(first + i * chunk_size));
      void* dest = voidify(*(result + i * chunk_size));
      std::memcpy(dest, src, n_bytes);
#endif
    }, 1);
    return {first + n, result + n};
  }
  // The next best thing -- if the iterators are random access we can still relocate everything in
  // parallel, just not by memcpying multiple objects at a time
  else if constexpr (random_access) {
    parallel_for(0, n, [&](size_t i){
      relocate_or_move_and_destroy(first[i], result[i]);
    });
    return {first + n, result + n};
  }
  // No parallelism allowed!
  else {
    for (; n > 0; ++result, (void)++first, --n) {
      // Note: Dereferencing result is UB since it points to uninitialized memory, but
      // I don't think that is avoidable until we get C++20 with std::to_address.
      relocate_or_move_and_destroy(*first, *result);
    }
    return {first, result};
  }
}


template<typename InputIterator, typename NoThrowForwardIterator>
NoThrowForwardIterator uninitialized_relocate(InputIterator first, InputIterator last, NoThrowForwardIterator result) {
  static_assert(parlay::is_input_iterator_v<InputIterator>);
  static_assert(parlay::is_forward_iterator_v<NoThrowForwardIterator>);

  if constexpr (is_random_access_iterator_v<InputIterator>) {
    return parlay::uninitialized_relocate_n(first, std::distance(first, last), result).second;
  }
  else {
    for (; first != last; ++result, (void)++first) {
      // Note: Dereferencing result is UB since it points to uninitialized memory, but
      // I don't think that is avoidable until we get C++20 with std::to_address.
      relocate_or_move_and_destroy(*first, *result);
    }
    return result;
  }
}

}  // namespace parlay

#endif  // PARLAY_RELOCATION_H_
