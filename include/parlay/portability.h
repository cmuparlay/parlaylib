
#ifndef PARLAY_PORTABILITY_H_
#define PARLAY_PORTABILITY_H_

#if defined(_WIN32)
#ifndef NOMINMAX
#define PARLAY_DEFINED_NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef PARLAY_DEFINED_NOMINMAX
#undef NOMINMAX
#endif
#endif

#include <cstdlib>

#include <iostream>

namespace parlay {

// PARLAY_INLINE: Ask the compiler politely to inline the given function.
#if defined(__GNUC__)
#define PARLAY_INLINE inline  __attribute__((__always_inline__))
#elif defined(_MSC_VER)
#define PARLAY_INLINE __forceinline
#else
#define PARLAY_INLINE inline
#endif

// PARLAY_NOINLINE: Ask the compiler to *not* inline the given function
#if defined(__GNUC__)
#define PARLAY_NOINLINE __attribute__((__noinline__))
#elif defined(_MSC_VER)
#define PARLAY_NOINLINE __declspec(noinline)
#else
#define PARLAY_NOINLINE
#endif

// PARLAY_COLD: Ask the compiler to place the given function far away from other code
#if defined(__GNUC__)
#define PARLAY_COLD __attribute__((__cold__))
#elif defined(_MSC_VER)
#define PARLAY_COLD
#else
#define PARLAY_COLD
#endif


// PARLAY_PACKED: Ask the compiler to pack a struct into less memory by not padding
#if defined(__GNUC__)
#define PARLAY_PACKED __attribute__((packed))
#else
#define PARLAY_PACKED
#endif

// PARLAY_NO_UNIQUE_ADDR: Allow a member object to occupy no space
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#define PARLAY_NO_UNIQUE_ADDR [[no_unique_address]]
#else
#define PARLAY_NO_UNIQUE_ADDR
#endif
#else
#define PARLAY_NO_UNIQUE_ADDR
#endif

// PARLAY_PREFETCH: Prefetch data into cache
#if defined(__GNUC__)
#define PARLAY_PREFETCH(addr, rw, locality) __builtin_prefetch ((addr), (rw), (locality))
#elif defined(_MSC_VER)
#define PARLAY_PREFETCH(addr, rw, locality)                                                 \
  PreFetchCacheLine(((locality) ? PF_TEMPORAL_LEVEL_1 : PF_NON_TEMPORAL_LEVEL_ALL), (addr))
#else
#define PARLAY_PREFETCH(addr, rw, locality)
#endif


#if defined(__cplusplus) && __cplusplus >= 202002L
#define PARLAY_LIKELY [[likely]]
#define PARLAY_UNLIKELY [[unlikely]]
#else
#define PARLAY_LIKELY
#define PARLAY_UNLIKELY
#endif

// Check for exceptions. The standard suggests __cpp_exceptions. Clang/GCC defined __EXCEPTIONS.
// MSVC disables them with _HAS_EXCEPTIONS=0.  Might not cover obscure compilers/STLs.
//
// Exceptions can be explicitly disabled in Parlay with PARLAY_NO_EXCEPTIONS.
#if !defined(PARLAY_NO_EXCEPTIONS) &&                            \
    ((defined(__cpp_exceptions) && __cpp_exceptions != 0) ||     \
     (defined(__EXCEPTIONS)) ||                                  \
     (defined(_HAS_EXCEPTIONS) && _HAS_EXCEPTIONS == 1) ||       \
     (defined(_MSC_VER) && !defined(_HAS_EXCEPTIONS)))
#define PARLAY_EXCEPTIONS_ENABLED
#endif

template<typename Exception, typename... Args>
[[noreturn]] PARLAY_NOINLINE PARLAY_COLD void throw_exception_or_terminate(Args&&... args) {
#if defined(PARLAY_EXCEPTIONS_ENABLED)
  throw Exception{std::forward<Args>(args)...};
#else
  std::cerr << Exception{std::forward<Args>(args)...}.what() << "\n";
  std::terminate();
#endif
}


}  // namespace parlay

#endif  // PARLAY_PORTABILITY_H_
