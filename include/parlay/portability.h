
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

namespace parlay {

// PARLAY_INLINE: Ask the compiler politely to inline the given function.
#if defined(__GNUC__)
#define PARLAY_INLINE inline  __attribute__((__always_inline__))
#elif defined(_MSC_VER)
#define PARLAY_INLINE __forceinline
#else
#define PARLAY_INLINE inline
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
#elif defined(_WIN32)
#define PARLAY_PREFETCH(addr, rw, locality)                                                 \
  PreFetchCacheLine(((locality) ? PF_TEMPORAL_LEVEL_1 : PF_NON_TEMPORAL_LEVEL_ALL), (addr))
#else
#define PARLAY_PREFETCH(addr, rw, locality)
#endif

}  // namespace parlay

#endif  // PARLAY_PORTABILITY_H_
