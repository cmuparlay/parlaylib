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
inline void atomic_source_memcpy(void* dest, void* source, size_t count, std::memory_order order = std::memory_order_acquire) {
  std::memcpy(dest, source, count);
  std::atomic_thread_fence(order);
}

inline void atomic_dest_memcpy(void* dest, void* source, size_t count, std::memory_order order = std::memory_order_release) {
  std::atomic_thread_fence(order);
  std::memcpy(dest, source, count);
}

// Basically std::bit_cast from C++20 but with a slightly different interface. The goal
// is to type pun from a byte representation into a valid object with valid lifetime.
template<typename T>
T bits_to_object(void* src) {
  union { std::monostate empty{}; T value; };
  std::memcpy(&value, src, sizeof(T));
  return value;
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

      alignas(T) char dest[sizeof(T)];
      internal::atomic_source_memcpy(&dest, &fast_buffer, sizeof(T));
      if (version.load(std::memory_order_relaxed) == num) return internal::bits_to_object<T>(dest);

      auto p = hazptr_instance().acquire(backup);
      T result{*p};
      hazptr_instance().release();
      return result;
    }
  }

  void store(const T& value) {
    auto num = version.load(std::memory_order_acquire);
    if (num % 2 == 0) {

      auto old_backup = backup.load();

      // Try to read the current fast value to back it up
      alignas(T) char dest[sizeof(T)];
      internal::atomic_source_memcpy(&dest, &fast_buffer, sizeof(T));
      if (version.load(std::memory_order_relaxed) == num) {
        // Successfully loaded fast value
        auto* new_backup = type_allocator<big_atomic_indirect<T>>::create(internal::bits_to_object<T>(dest));


        // Try to back it up
        if (backup.compare_exchange_strong(old_backup, new_backup)) {
          // Successfully installed backup value

          // Successfully took the lock
          auto expected = num;
          if (version.compare_exchange_strong(expected, num + 1)) {

          }
          else if (expected >= num + 2) {
            // Someone else has completed copied a new value
          }
          else {
            // Someone has a copy in flight

          }


        }
        else {
          // Failed backup -- another thread is racing us
          type_allocator<big_atomic_indirect<T>>::destroy(new_backup);

        }

      }

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

 private:

  T attempt_fast_read() {

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
