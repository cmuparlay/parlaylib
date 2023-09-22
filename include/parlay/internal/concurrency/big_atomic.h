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
T bits_to_object(char* src) {
  union { std::monostate empty{}; T value; };
  std::memcpy(&value, src, sizeof(T));
  return value;
}

template<typename T>
struct big_atomic_indirect {
  T value;
  big_atomic_indirect<T>* next_;    // Intrusive link for hazard pointers
};

template<typename T>
big_atomic_indirect<T>* intrusive_get_next(big_atomic_indirect<T>* p) {
  return p->next_;
}

template<typename T>
void intrusive_set_next(big_atomic_indirect<T>* p, big_atomic_indirect<T>* next) {
  p->next_ = next;
}

}  // namespace internal



template<typename T>
struct big_atomic {
  // T must be trivially copyable, but it doesn't have to be trivially default
  // constructible since we only make copies from what the user gives us
  static_assert(std::is_trivially_copyable_v<T>);

  using version_type = std::size_t;
  using indirect_type = internal::big_atomic_indirect<T>;

  using allocator = type_allocator<indirect_type>;

  big_atomic() : version(0), indirect_value(nullptr), fast_value{} {
    hazptr_instance();  // force correct static initialization order
    T t{};              // Default construct the initial value
    std::memcpy(&fast_value, &t, sizeof(T));
  }

  explicit big_atomic(T t) : version(0), indirect_value(nullptr), fast_value{} {
    hazptr_instance();  // force correct static initialization order
    std::memcpy(&fast_value, &t, sizeof(T));
  }

  T load() {
    auto num = version.load(std::memory_order_acquire);
    if (num % 2 == 0) {
      alignas(T) char dest[sizeof(T)];
      internal::atomic_source_memcpy(&dest, &fast_value, sizeof(T));
      if (version.load(std::memory_order_relaxed) == num) return internal::bits_to_object<T>(dest);
    }
    auto p = hazptr_instance().acquire(indirect_value);
    T result{*p};
    hazptr_instance().release();
    return result;
  }

  void store(const T& value) {
    auto num = version.load(std::memory_order_acquire);
    if (num % 2 == 0) {

      auto current_indirect = indirect_value.load();

      // Try to read the current fast value to back it up
      alignas(T) char dest[sizeof(T)];
      internal::atomic_source_memcpy(&dest, &fast_value, sizeof(T));

      // Check that the object is unlocked, so we can assume the fast value is not torn
      if (version.load(std::memory_order_relaxed) == num) {
        // Successfully loaded fast value while unlocked.  Try to back it up, so we can proceed to store
        auto* backup = type_allocator<indirect_type>::create(internal::bits_to_object<T>(dest));

        // Try to back it up
        if (indirect_value.compare_exchange_strong(current_indirect, backup)) {
          // Successfully installed backup value.
          // Retire the old backup that we replaced
          hazptr_instance().retire(current_indirect);

          auto expected = num;
          if (version.compare_exchange_strong(expected, num + 1)) {
              // Successfully acquired the lock
              // Store the new fast copy and unlock
              internal::atomic_dest_memcpy(&fast_value, &value, sizeof(T));
              version.store(num + 2, std::memory_order_release);
              return;
          }
          else if (expected == num + 2) {
            // Someone else took the lock and stored their own value.
            // In this case we can linearize before that store and
            // just do nothing!!
            return;
          }
          else if (expected == num + 1) {
            // Fallthrough to bottom case
          }
        }
        else {
          // Failed backup -- another thread is racing us
          allocator::destroy(backup);

          // Fallthrough to bottom case
        }

      }
      else {
        // We got torn when trying to read for the backup. A store is in progress or
        // has already finished

        // Fallthrough to bottom case
      }
    }
    else {
        // The object is already locked.  This means that the backup already contain(ed) a valid
        // backup of the previous fast value.

      // Fallthrough to bottom case
    }

    // Someone else has acquired the lock.  We can not just linearize before them
    // because their copy might not be done yet, so we need to at least try to
    // make our store visible to the world.  Since the object is in locked mode,
    // we can just store ourselves in the backup and that is good enough to then
    // linearize before the in-flight store.

    auto new_indirect = allocator::create(value);
    auto old_indirect = indirect_value.exchange(new_indirect);  // <-- this is sketchy. need to prove that it isn't wrong.

    // We have successfully installed our value while the object is locked, so
    // it is visible to readers, and we can linearize before the in-flight store'
    if (old_indirect) {
      hazptr_instance().retire(old_indirect);
    }
  }

 private:

  static internal::intrusive_acquire_retire<indirect_type>& hazptr_instance() {
    static internal::intrusive_acquire_retire<indirect_type> instance;
    return instance;
  }

  std::atomic<version_type> version;
  std::atomic<indirect_type*> indirect_value{nullptr};
  char fast_value[sizeof(T)];
};



}  // namespace parlay
