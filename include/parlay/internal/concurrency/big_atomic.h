#pragma once

#include <cstddef>
#include <cstring>

#include <atomic>
#include <utility>

#include "../../utilities.h"

#include "acquire_retire.h"

#include "../../alloc.h"

namespace parlay {

namespace internal {

// Pretend implementation of P1478R1: Byte-wise atomic memcpy. Technically undefined behavior
// since std::memcpy is not immune to data races, but on most hardware we should be okay. In
// C++26 we can probably do this for real (assuming Concurrency TS 2 is accepted).
void atomic_source_memcpy(void* dest, void* source, size_t count, std::memory_order order = std::memory_order_acquire) {
  std::memcpy(dest, source, count);
  std::atomic_thread_fence(order);
}

void atomic_dest_memcpy(void* dest, void* source, size_t count, std::memory_order order = std::memory_order_release) {
  std::atomic_thread_fence(order);
  std::memcpy(dest, source, count);
}


}  // namespace internal

template<typename T>
struct big_atomic_indirect {

  T value;
  big_atomic_indirect<T>* next_;    // Intrusive link for hazard pointers
};

template<typename T>
struct big_atomic {
  static_assert(std::is_trivially_copyable_v<T>);

  big_atomic() {
    hazptr_instance();  // force correct static initialization order
  }

  using version_type = std::size_t;

  T load() {
    auto num = version.load(std::memory_order_acquire);
    if (num % 2 == 0) {
      if constexpr (std::is_trivial_v<T>) {
        T result;  // Only valid since we are assuming T is trivial
        internal::atomic_source_memcpy(&result, &fast_buffer, sizeof(T));
        if (version.load(std::memory_order_relaxed) == num) return result;
      }
      else {
        alignas(T) char dest[sizeof(T)];    // If T is not trivial, we can't default construct one
        internal::atomic_source_memcpy(&dest, &fast_buffer, sizeof(T));
        if (version.load(std::memory_order_relaxed) == num) return T(from_bytes<T>(&dest));
      }
      auto p = hazptr_instance().acquire(backup);
      T result{*p};
      hazptr_instance().release();
      return result;
    }
  }

  void store(const T& value) {
    auto num = version.load(std::memory_order_acquire);
    if (num % 2 == 0) {
      // Attempt to take the seqlock and store a value
      auto* indirect_val = type_allocator<big_atomic_indirect<T>>::create(value);
      auto old = backup.load();
      if (backup.compare_exchange_strong(old, indirect_val)) {
        // Successfully installed backup value

        if (version.compare_exchange_strong(num, num + 1)) {
          // Successfully took the lock
          internal::atomic_dest_memcpy(&fast_buffer, &value, sizeof(T));
          version.store(num + 2, std::memory_order_release);
          if (old) {
            hazptr_instance().retire(old);
          }
          return;
        }

      }

    }
    if (num % 2 == 1) {



    }
    for (;;) {
      auto indirect_val = type_allocator<big_atomic_indirect<T>>::create(value);

    }
  }

  static internal::intrusive_acquire_retire<big_atomic_indirect<T>>& hazptr_instance() {
    static internal::intrusive_acquire_retire<big_atomic_indirect<T>> instance;
    return instance;
  }

  std::atomic<big_atomic_indirect<T>*> backup{nullptr};
  std::atomic<version_type> version;
  char fast_buffer[sizeof(T)];
};






}  // namespace parlay
