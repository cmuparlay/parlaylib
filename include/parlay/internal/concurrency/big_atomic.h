#pragma once

#include <cstddef>
#include <cstring>

#include <atomic>
#include <utility>

#include "../../utilities.h"

#include "acquire_retire.h"
#include "marked_ptr.h"

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
  explicit big_atomic_indirect(const T& value_) : value(value_) { }
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


// The locking mechanism goes in this order:
//  - sets the dirty bit on the ptr,
//    - takes the seqlock
//      - writes the new fast value
//    - releases the seqlock
//  - clears the dirty bit on the ptr
//
// The seqlock is strict in that only one thread can
// acquire it, and that thread will release it, but
// the dirty bit can be changed by a racing thread.


template<typename T>
struct big_atomic {
  // T must be trivially copyable, but it doesn't have to be trivially default
  // constructible since we only make copies from what the user gives us
  static_assert(std::is_trivially_copyable_v<T>);

  using version_type = std::size_t;
  using indirect_type = internal::big_atomic_indirect<T>;
  using marked_indirect_ptr = marked_ptr<indirect_type>;

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
    alignas(T) char dest[sizeof(T)];
    internal::atomic_source_memcpy(&dest, &fast_value, sizeof(T));
    auto p = indirect_value.load();
    if (p.get_mark() == 0 && num == version.load(std::memory_order_relaxed)) {
      return internal::bits_to_object<T>(dest);
    }
    else {
      auto hazptr = hazptr_holder{};
      auto p = hazptr.protect(indirect_value);
      assert(p != nullptr);
      return *p;
    }
  }


  void store(const T& desired) {
    auto num = version.load(std::memory_order_acquire);
    auto new_p = marked_indirect_ptr(allocator::create(desired)).set_mark(1);
    auto old_p = indirect_value.exchange(new_p);
    retire(old_p);
    sync_to_fast(num, desired, new_p);
  }


  bool compare_and_swap(const T& expected, const T& desired) {
    auto num = version.load(std::memory_order_acquire);
    
    alignas(T) char dest[sizeof(T)];
    internal::atomic_source_memcpy(&dest, &fast_value, sizeof(T));
    auto p = indirect_value.load(std::memory_order_acquire);
    
    bool fast_valid = (p.get_mark() == 0 && num == version.load(std::memory_order_relaxed));

    auto hazptr = hazptr_holder{};
    p = (fast_valid) ? p : hazptr.protect(indirect_value);
    T current = (fast_valid) ? internal::bits_to_object<T>(dest) : *p;

    if (current != expected) {
      return false;
    }
    else if (current == expected && expected == desired) {
      return true;
    }
    
    auto new_p = marked_indirect_ptr(allocator::create(desired)).set_mark(1);
    auto old_p = p;
    
    if (indirect_value.compare_exchange_strong(p, new_p)
         || (p == old_p.remove_mark() && indirect_value.compare_exchange_strong)) {
           
      retire(p);
      sync_to_fast(num, desired, new_p);
      return true;
    }
    else {
      allocator::destroy(new_p.clear_mark());
      return false;
    }
  }

 private:
  
  void sync_to_fast(version_type num, const T& desired, marked_indirect_ptr p) noexcept {
    if ((num % 2 == 0) && version.compare_exchange_strong(num, num + 1)) {
      internal::atomic_dest_memcpy(&fast_value, &desired, sizeof(T));
      version.store(num + 2, std::memory_order_release);
      indirect_value.compare_exchange_strong(p, p.remove_mark());
    }
  }
  
  class hazptr_holder {
    marked_indirect_ptr protect(const std::atomic<marked_indirect_ptr>& src) const {
      return hazptr_instance().acquire(src);
    }
    
    ~hazptr_holder() {
      hazptr_instance().release();
    }
  };

  static void retire(T* p) {
    if (p) {
      hazptr_instance().retire(p);
    }
  }

  static internal::intrusive_acquire_retire<indirect_type>& hazptr_instance() {
    static internal::intrusive_acquire_retire<indirect_type> instance;
    return instance;
  }

  std::atomic<version_type> version;
  std::atomic<marked_indirect_ptr> indirect_value{nullptr};
  char fast_value[sizeof(T)];
};



}  // namespace parlay
