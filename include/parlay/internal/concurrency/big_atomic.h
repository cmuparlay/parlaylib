#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>

#include "../../alloc.h"

#include "acquire_retire.h"
#include "marked_ptr.h"

namespace parlay {

namespace internal {

// Pretend implementation of P1478R1: Byte-wise atomic memcpy. Technically undefined behavior
// since std::memcpy is not immune to data races, but on most hardware we should be okay. In
// C++26 we can probably do this for real (assuming Concurrency TS 2 is accepted).
inline void atomic_load_per_byte_memcpy(void* dest, const void* source, size_t count, std::memory_order order = std::memory_order_acquire) {
  std::memcpy(dest, source, count);
  std::atomic_thread_fence(order);
}

inline void atomic_store_per_byte_memcpy(void* dest, const void* source, size_t count, std::memory_order order = std::memory_order_release) {
  std::atomic_thread_fence(order);
  std::memcpy(dest, source, count);
}

// Basically std::bit_cast from C++20 but with a slightly different interface. The goal
// is to type pun from a byte representation into a valid object with valid lifetime.
template<typename T>
T bits_to_object(const char* src) {
  struct empty{};
  union { empty empty_{}; T value; };
  std::memcpy(&value, src, sizeof(T));
  return value;
}

}  // namespace internal



template<typename T>
struct big_atomic {
  // T must be trivially copyable, but it doesn't have to be trivially default constructible (or
  // even default constructible at all) since we only make copies from what the user gives us
  static_assert(std::is_trivially_copyable_v<T>);

 private:
  struct indirect_holder {
    explicit indirect_holder(const T& value_) : value(value_) { }
    T value;
    indirect_holder* next_;    // Intrusive link for hazard pointers
  };

  friend indirect_holder* intrusive_get_next(indirect_holder* p) {
    return p->next_;
  }

  friend void intrusive_set_next(indirect_holder* p, indirect_holder* next) {
    p->next_ = next;
  }

 public:

  using version_type = std::size_t;
  using marked_indirect_ptr = marked_ptr<indirect_holder>;

  using allocator = type_allocator<indirect_holder>;

  big_atomic() : version(0), indirect_value(nullptr), fast_value{} {
    static_assert(std::is_default_constructible_v<T>);

    // force correct static initialization order
    allocator().init();
    hazptr_instance();

    T t{};              // Default construct the initial value
    std::memcpy(&fast_value, &t, sizeof(T));
  }

  explicit big_atomic(T t) : version(0), indirect_value(nullptr), fast_value{} {
    // force correct static initialization order
    allocator().init();
    hazptr_instance();

    std::memcpy(&fast_value, &t, sizeof(T));
  }

  T load() {
    auto reader = read_fast();
    if (reader.fast_valid) {
      return reader.get_fast();
    }
    else {
      auto hazptr = hazptr_holder{};
      auto p = hazptr.protect(indirect_value);
      assert(p != nullptr);
      // Readers can help put the object back into "fast mode".  This is very good if updates
      // are infrequent since this will speed up all subsequent reads.  Unfortunately this
      // could be a severe slowdown is updates are very frequent, since many reads will try
      // to help and waste work here...  Might be nice to optimize this so that it doesn't
      // try to help every time, but only every once in a while.
      if (p.get_mark() == SLOW_MODE) {
        try_seqlock_and_store(reader.num, p->value, p);
      }
      return p->value;
    }
  }

  void store(const T& desired) {
    auto num = version.load(std::memory_order_acquire);
    auto new_p = marked_indirect_ptr(allocator::create(desired)).set_mark(SLOW_MODE);
    auto old_p = indirect_value.exchange(new_p);
    retire(old_p);
    try_seqlock_and_store(num, desired, new_p);
  }

  bool compare_and_swap(const T& expected, const T& desired) {
    auto reader = read_fast();

    auto hazptr = hazptr_holder{};
    auto p = (reader.fast_valid) ? reader.p : hazptr.protect(indirect_value);
    T current = (reader.fast_valid) ? reader.get_fast() : p->value;

    if (!(current == expected)) {
      return false;
    }
    else if (current == expected && expected == desired) {
      return true;
    }
    
    auto new_p = marked_indirect_ptr(allocator::create(desired)).set_mark(SLOW_MODE);
    auto old_p = p;
    
    if (indirect_value.compare_exchange_strong(p, new_p)
         || (p == old_p.clear_mark() && indirect_value.compare_exchange_strong(p, new_p))) {
           
      retire(p);
      try_seqlock_and_store(reader.num, desired, new_p);
      return true;
    }
    else {
      allocator::destroy(new_p.clear_mark());
      return false;
    }
  }

  ~big_atomic() {
    auto p = indirect_value.load(std::memory_order_acquire);
    if (p) {
      allocator::destroy(p);
    }
  }

 private:

  struct fast_reader {

    T get_fast() const noexcept {
      assert(fast_valid);
      return internal::bits_to_object<T>(buffer);
    }

    bool fast_valid;
    version_type num;
    marked_indirect_ptr p;
    alignas(T) char buffer[sizeof(T)];
  };

  fast_reader read_fast() {
    fast_reader loader;
    loader.num = version.load(std::memory_order_acquire);
    internal::atomic_load_per_byte_memcpy(&loader.buffer, &fast_value, sizeof(T));
    loader.p = indirect_value.load();
    loader.fast_valid = loader.p.get_mark() != SLOW_MODE && loader.num == version.load(std::memory_order_relaxed);
    return loader;
  }

  void try_seqlock_and_store(version_type num, const T& desired, marked_indirect_ptr p) noexcept {
    if ((num % 2 == 0) && version.compare_exchange_strong(num, num + 1)) {
      internal::atomic_store_per_byte_memcpy(&fast_value, &desired, sizeof(T));
      version.store(num + 2, std::memory_order_release);
      indirect_value.compare_exchange_strong(p, p.clear_mark());
    }
  }

  struct hazptr_holder {
    marked_indirect_ptr protect(const std::atomic<marked_indirect_ptr>& src) const {
      return hazptr_instance().acquire(src);
    }
    
    ~hazptr_holder() {
      hazptr_instance().release();
    }
  };

  static void retire(indirect_holder* p) {
    if (p) {
      hazptr_instance().retire(p);
    }
  }

  static constexpr auto dealloc = [](indirect_holder* p) { allocator::destroy(p); };

  static internal::intrusive_acquire_retire<indirect_holder, decltype(dealloc)>& hazptr_instance() {
    static internal::intrusive_acquire_retire<indirect_holder, decltype(dealloc)> instance(dealloc);
    return instance;
  }

  static constexpr uintptr_t SLOW_MODE = 1;

  std::atomic<version_type> version;
  std::atomic<marked_indirect_ptr> indirect_value{nullptr};
  char fast_value[sizeof(T)];
};


}  // namespace parlay
