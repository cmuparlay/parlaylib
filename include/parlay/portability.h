
#ifndef PARLAY_PORTABILITY_H_
#define PARLAY_PORTABILITY_H_

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


}  // namespace parlay

#endif  // PARLAY_PORTABILITY_H_
