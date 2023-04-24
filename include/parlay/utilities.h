
#ifndef PARLAY_UTILITIES_H_
#define PARLAY_UTILITIES_H_

#include <cstddef>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>

#include <algorithm>
#include <atomic>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "portability.h"
#include "parallel.h"
#include "range.h"           // IWYU pragma: keep
#include "type_traits.h"

#include "internal/debug_uninitialized.h"

namespace parlay {

template <typename Lf, typename Rf>
static void par_do_if(bool do_parallel, Lf left, Rf right, bool cons = false) {
  if (do_parallel)
    par_do(left, right, cons);
  else {
    left();
    right();
  }
}

template <typename Lf, typename Mf, typename Rf>
inline void par_do3(Lf left, Mf mid, Rf right) {
  auto left_mid = [&]() { par_do(left, mid); };
  par_do(left_mid, right);
}

template <typename Lf, typename Mf, typename Rf>
static void par_do3_if(bool do_parallel, Lf left, Mf mid, Rf right) {
  if (do_parallel)
    par_do3(left, mid, right);
  else {
    left();
    mid();
    right();
  }
}

// Obtains a pointer to an object of type T located at the address represented
// by p. Essentially performs std::launder(reinterpret_cast<T*>(p)).
template<typename T>
[[nodiscard]] constexpr T* from_bytes(std::byte* p) noexcept {
  // std::launder not available on older compilers
#ifdef __cpp_lib_launder
  return std::launder(reinterpret_cast<T*>(p));
#else
  return reinterpret_cast<T*>(p);
#endif
}


template <class T>
size_t log2_up(T);

struct empty {};

typedef uint32_t flags;
const flags no_flag = 0;
const flags fl_sequential = 1;
const flags fl_debug = 2;
const flags fl_time = 4;
const flags fl_conservative = 8;
const flags fl_inplace = 16;

template <typename T>
inline void assign_uninitialized(T& a, const type_identity_t<T>& b) {
  PARLAY_ASSERT_UNINITIALIZED(a);
  new (static_cast<T*>(std::addressof(a))) T(b);
}

template <typename T>
inline auto assign_uninitialized(T& a, type_identity_t<T>&& b) {
  PARLAY_ASSERT_UNINITIALIZED(a);
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));
}

template <typename T>
inline void move_uninitialized(T& a, type_identity_t<T>& b) {
  PARLAY_ASSERT_UNINITIALIZED(a);
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));
}

/* Hashing functions for various integer types */

// a 32-bit hash function
inline uint32_t hash32(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint32_t hash32_2(uint32_t a) {
  uint32_t z = (a + 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

inline uint32_t hash32_3(uint32_t a) {
  uint32_t z = a + 0x9e3779b9;
  z ^= z >> 15;  // 16 for murmur3
  z *= 0x85ebca6b;
  z ^= z >> 13;
  z *= 0xc2b2ae3d;  // 0xc2b2ae35 for murmur3
  return z ^ (z >> 16);
}

// from numerical recipes
inline uint64_t hash64(uint64_t u) {
  uint64_t v = u * 3935559000370003845ul + 2691343689449507681ul;
  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >> 4;
  v *= 4768777513237032717ul;
  v ^= v << 20;
  v ^= v >> 41;
  v ^= v << 5;
  return v;
}

// a slightly cheaper, but possibly not as good version
// based on splitmix64
inline uint64_t hash64_2(uint64_t x) {
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  x = x ^ (x >> 31);
  return x;
}

// Combine two hash values into a single hash value. Formula borrowed from Boost
inline void hash_combine(size_t& seed, size_t v) {
  seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// parlay::hash provides a customization point for hashing that allows
// us to override the hash for standard types, or to provide specializations
// for standard types that do not have a std::hash specialization themselves.
template<typename T, typename Enable = void>
struct hash : public std::hash<T> { };

template <typename T>
struct hash<T, typename std::enable_if_t<std::is_integral_v<T>>> {
  size_t operator()(const T& p) const {
    return p * UINT64_C(0xbf58476d1ce4e5b9);
  }
};
    
// Hashing for std::pairs
template<typename U, typename V>
struct hash<std::pair<U,V>> {
  size_t operator()(const std::pair<U,V>& p) const {
    size_t h = parlay::hash<U>{}(p.first);
    hash_combine(h, parlay::hash<V>{}(p.second));
    return h;
  }
};

template<typename V, std::size_t n>
struct hash<std::array<V,n>> {
  size_t operator()(const std::array<V,n>& a) const {
    size_t h = 1;
    for (int i=0; i < n; i++) {
      hash_combine(h, parlay::hash<V>{}(a[i]));
    }
    return h;
  }
};

/* Atomic write-add, write-min, and write-max */

template <typename T, typename EV>
inline void write_add(std::atomic<T>* a, EV b) {
  T newV, oldV;
  do {
    oldV = a->load();
    newV = oldV + b;
  } while (!std::atomic_compare_exchange_weak(a, &oldV, newV));
}

template <typename T, typename F>
inline bool write_min(std::atomic<T>* a, T b, F less) {
  T c;
  bool r = false;
  do {
    c = a->load();
  } while (less(b, c) && !(r = std::atomic_compare_exchange_weak(a, &c, b)));
  return r;
}

template <typename T, typename F>
inline bool write_max(std::atomic<T>* a, T b, F less) {
  T c;
  bool r = false;
  do {
    c = a->load();
  } while (less(c, b) && !(r = std::atomic_compare_exchange_weak(a, &c, b)));
  return r;
}

// returns the log base 2 rounded up (works on ints or longs or unsigned
// versions)
template <class T>
size_t log2_up(T i) {
  assert(i > 0);
  size_t a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    a++;
  }
  return a;
}

inline size_t granularity(size_t n) {
  return (n > 100) ? static_cast<size_t>(std::ceil(std::sqrt(n))) : 100;
}

//  A copyable_function_wrapper allows an object to store a function (e.g., a lambda,
//  or any callable object) as a member while still being default copy-assignable.
//
//  As long as the function type is copy constructible (which it needs to be in
//  order to even initialize the member), this wrapper will be copy assignable.
template<typename F>
struct copyable_function_wrapper {

  static_assert(std::is_copy_constructible_v<F>);

  explicit copyable_function_wrapper(F _f) : f(std::move(_f)) {}

  copyable_function_wrapper(const copyable_function_wrapper&) = default;
  copyable_function_wrapper(copyable_function_wrapper&&)
    noexcept(std::is_nothrow_move_constructible_v<F>) = default;

  copyable_function_wrapper& operator=(const copyable_function_wrapper& other) {
    if constexpr (std::is_copy_assignable_v<F>) {
      f = other.f;
    }
    else {
      f.~F();
      new (std::addressof(f)) F(other.f);
    }
    return *this;
  }

  copyable_function_wrapper& operator=(copyable_function_wrapper&& other)
    noexcept(std::is_nothrow_move_assignable_v<F> && std::is_nothrow_move_constructible_v<F>) {
    if constexpr (std::is_move_assignable_v<F>) {
      f = std::move(other.f);
    }
    else {
      f.~F();
      new (std::addressof(f)) F(std::move(other.f));
    }
    return *this;
  }

  ~copyable_function_wrapper() = default;

  template<typename... Args>
  PARLAY_INLINE decltype(auto) operator()(Args&&... args) noexcept(std::is_nothrow_invocable_v<F&, Args...>) {
    static_assert(std::is_invocable_v<F&, Args...>);
    return std::invoke(f, std::forward<Args>(args)...);
  }

  template<typename... Args>
  PARLAY_INLINE decltype(auto) operator()(Args&&... args) const noexcept(std::is_nothrow_invocable_v<const F&, Args...>) {
    static_assert(std::is_invocable_v<const F&, Args...>);
    return std::invoke(f, std::forward<Args>(args)...);
  }

  // Special case for when the argument is just a size_t. This is the common case since this is what
  // all the functions in delayed_sequence are. This should in theory make absolutely no difference
  // compared to the templates whatsoever but for some reason it's faster???
  PARLAY_INLINE decltype(auto) operator()(size_t i) noexcept(std::is_nothrow_invocable_v<F&, size_t>) {
    static_assert(std::is_invocable_v<F&, size_t>);
    return std::invoke(f, i);
  }

  PARLAY_INLINE decltype(auto) operator()(size_t i) const noexcept(std::is_nothrow_invocable_v<const F&, size_t>) {
    static_assert(std::is_invocable_v<const F&, size_t>);
    return std::invoke(f, i);
  }

  F* get() { return std::addressof(f); }
  const F* get() const { return std::addressof(f); }

 private:
  F f;
};



/* Relocation (a.k.a. "destructive move")

   The relocation of object a into memory b is equivalent to a move
   construction of a into b, followed by the destruction of what
   remains at a. In other words, it is

   new (b) T(std::move(*a));
   a->~T();

   For many types, however, this can be optimized by replacing it
   with just

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

// Relocate a single object into uninitialized memory, leaving
// the source memory uninitialized afterwards.
template<typename T>
inline void uninitialized_relocate(T* to, T* from) noexcept(is_nothrow_relocatable<T>::value) {
  if constexpr (is_trivially_relocatable<T>::value) {
    std::memcpy(static_cast<void*>(to), static_cast<void*>(from), sizeof(T));
  }
  else {
    static_assert(std::is_move_constructible<T>::value);
    static_assert(std::is_destructible<T>::value);
    PARLAY_ASSERT_UNINITIALIZED(*to);
    ::new (to) T(std::move(*from));
    from->~T();
    PARLAY_ASSERT_UNINITIALIZED(*from);
  }
}

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
  // trivially relocatable, or because the allocator has specialize behaviour. We now fall
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


/* For inplace sorting / merging, we sometimes need to move values
   around and sometimes we want to make copies. We use tag dispatch
   to choose between moving and copying, so that the move algorithm
   can be written agnostic to which one it uses. We also account for
   moving / copying between uninitialized memory.
   
   Usage:
     assign_dispatch(dest, val, tag_type())

   Tag types:
    move_assign_tag:                 The value is moved assigned into dest from val
    uninitialized_move_tag:          The value is move constructed from val into dest
    copy_assign_tag:                 The value is copy assigned into dest from val
    uninitialized_copy_tag:          The value is copy constructed from val into dest
    uninitialized_relocate_tag:      The value is destructively moved from val into dest
   
*/   

struct move_assign_tag {};
struct uninitialized_move_tag {};
struct copy_assign_tag {};
struct uninitialized_copy_tag {};
struct uninitialized_relocate_tag {};

// Move dispatch -- move val into dest
template<typename T>
void assign_dispatch(T& dest, type_identity_t<T>& val, move_assign_tag) {
  dest = std::move(val);
}

// Copy dispatch -- copy val into dest
template<typename T>
void assign_dispatch(T& dest, const type_identity_t<T>& val, copy_assign_tag) {
  dest = val;
}

// Uninitialized move dispatch -- move construct dest with val
template<typename T>
void assign_dispatch(T& dest, type_identity_t<T>& val, uninitialized_move_tag) {
  assign_uninitialized(dest, std::move(val));
}

// Uninitialized copy dispatch -- copy initialize dest with val
template<typename T>
void assign_dispatch(T& dest, const type_identity_t<T>& val, uninitialized_copy_tag) {
  assign_uninitialized(dest, val);
}

// Uninitialized relocate dispatch -- destructively move val into dest
template<typename T>
void assign_dispatch(T& dest, T& val, uninitialized_relocate_tag) {
  uninitialized_relocate(&dest, &val);
}

namespace internal {

// The deleter used by unique_array / make_unique_array
template<typename T>
struct unique_array_deleter {
  void operator()(T* ptr) {
    if (ptr != nullptr) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        parallel_for(0, n, [ptr](size_t i) {
          ptr[i].~T();
        });
      }
      ::operator delete (static_cast<void*>(ptr), std::align_val_t{alignof(T)});
    }
  }
  size_t n;
};

}  // namespace internal

// An alias of unique_ptr<T[], *custom_deleter*> as returned by make_unique_array.
// Stores a dynamically allocated array of type T[] that is automatically freed when
// the unique_ptr goes out of scope. Unlike regular unique_ptr<T[]>, this type with
// make_unique_array can be used to construct an array of non-default-constructible T
template<typename T>
using unique_array = std::unique_ptr<T[], internal::unique_array_deleter<T>>;


// Creates a unique_ptr<T[], *custom_deleter*> consisting of n objects of
// type T initialized by T(init(i)) for i from 0 to n - 1. The objects are
// automatically destroyed and freed when the unique_ptr goes out of scope.
//
// Useful in place of unique_ptr<T[]> when T is not default constructible
// and hence make_unique<T[]> can not be used.
template<typename T, typename F>
auto make_unique_array(size_t n, F init) {
  // Either T is constructible from init(i), or init(i) returns a prvalue T which means
  // we get guaranteed copy elision and can construct T even if it isn't copyable/movable
  static_assert(std::is_constructible_v<T, std::invoke_result_t<F&, size_t>> ||
                std::is_same_v<T, std::invoke_result_t<F&, size_t>>);

  if (n == 0) return unique_array<T>(nullptr, {0});
  auto buffer = static_cast<std::byte*>(::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
  parallel_for(1, n, [&](size_t i) {
    new (buffer + i * sizeof(T)) T(init(i));  // NOLINT
  });
  auto first = new (buffer) T(init(0));
  return unique_array<T>(first, {n});
}


/*
    Padded types for avoiding false sharing by over-aligning to a cache-line boundary.

    parlay::padded<T> should be a drop-in replacement for T in just about any scenario,
    without having to change the code that makes use of the T object. padded<T> can be
    initialized with any initializer that would work on T, including copy and move
    initialization, and any constructor of T. Assignment operations should also work.

    You can also bind a padded<T> to const/non-const reference/rvalue-reference variables
    of type T, e.g.,

      padded<int> p{1};
      int& x = p;  // refers to the padded int 1
      x = 2;       // p now contains 2

    This also applies to function parameters. For class types, you should also be able
    to invoke overloaded operations and member functions, e.g.,

      padded<std::vector<int>> v{1,2,3};
      v.push_back(4);
      v[0] = 42;

    You can even invoke function pointers!!

      padded<int(*)()> fp = &func;
      int x = fp();

    The only thing that doesn't seem to work is directly invoking member-function pointers...

 */

// Padded types to avoid false sharing. Takes a type T and makes its size
// at least Size bytes by over-aligning it. Size must be a valid alignment.
template<typename T, size_t Size = 128, typename Sfinae = void>
struct padded;

// Use user-defined conversions to pad primitive types
template<typename T, size_t Size>
struct alignas(Size) padded<T, Size, typename std::enable_if_t<std::is_scalar_v<T>>> {
  padded() : x{} {};
  /* implicit */ padded(T _x) : x(_x) { }       // cppcheck-suppress noExplicitConstructor    // NOLINT
  /* implicit */ operator T&() & { return x; }                                                // NOLINT
  /* implicit */ operator const T&() const& { return x; }                                     // NOLINT
  /* implicit */ operator T&&() && { return std::move(x); }                                   // NOLINT
  /* implicit */ operator const T&&() const&& { return std::move(x); }                        // NOLINT

  // Allow padded function pointers to be directly invoked!
  template<typename... Ts> auto operator()(Ts... args) const noexcept(std::is_nothrow_invocable_v<T, Ts...>)
      -> std::enable_if_t<std::is_invocable_v<T, Ts...>, std::invoke_result_t<T, Ts...>> {
    return x(std::forward<Ts>(args)...);
  }

  // Note: I couldn't figure out a way to make member-function pointers directly invocable :(

  T x;
};

// Use inheritance to pad class types
template<typename T, size_t Size>
struct alignas(Size) padded<T, Size, typename std::enable_if_t<std::is_class_v<T>>> : public T {
  static_assert(std::is_same_v<T, std::remove_cv_t<T>>,
      "padded<T> requires T be non-const-volatile qualified. Use const padded<T> instead of padded<const T>");
  using T::T;                                   // inherit (non-special) constructors
  padded() = default;                           // implement non-inherited special constructors
  /* implicit */ padded(const T& t) : T(t) {}         // cppcheck-suppress noExplicitConstructor          // NOLINT
  /* implicit */ padded(T&& t) : T(std::move(t)) {}   // cppcheck-suppress noExplicitConstructor          // NOLINT
};

}  // namespace parlay

#endif  // PARLAY_UTILITIES_H_
