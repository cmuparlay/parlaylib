#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <atomic>
#include <bit>

#include "../../alloc.h"

//#include "acquire_retire.h"
#include "hazard_ptr.h"
#include "marked_ptr.h"

namespace parlay {

namespace internal {

// Pretend implementation of P1478R1: Byte-wise atomic memcpy. Technically undefined behavior
// since std::memcpy is not immune to data races, but on most hardware we should be okay. In
// C++26 we can probably do this for real (assuming Concurrency TS 2 is accepted).
PARLAY_INLINE void atomic_load_per_byte_memcpy(void* dest, const void* source, size_t count, std::memory_order order = std::memory_order_acquire) {
  std::memcpy(dest, source, count);
  std::atomic_thread_fence(order);
}

PARLAY_INLINE void atomic_store_per_byte_memcpy(void* dest, const void* source, size_t count, std::memory_order order = std::memory_order_release) {
  std::atomic_thread_fence(order);
  std::memcpy(dest, source, count);
}

// Basically std::bit_cast from C++20 but with a slightly different interface. The goal
// is to type pun from a byte representation into a valid object with valid lifetime.
template<typename T>
PARLAY_INLINE T bits_to_object(const char* src) {
  return *reinterpret_cast<const T*>(src);  // Evil, scary, undefined behaviour!!! But its 20% faster :'(
  /*
  if constexpr(!std::is_trivially_default_constructible_v<T>) {
    struct empty{};
    union { empty empty_{}; T value; };
    std::memcpy(&value, src, sizeof(T));
    return value;
  }
  else {
    T value;
    std::memcpy(&value, src, sizeof(T));
    return value;
  }
  */
}

}  // namespace internal



template<typename T, typename Equal = std::equal_to<>>
struct big_atomic {
  // T must be trivially copyable, but it doesn't have to be trivially default constructible (or
  // even default constructible at all) since we only make copies from what the user gives us
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::is_invocable_r_v<bool, Equal, T&&, T&&>);

 private:
  struct indirect_holder {
    explicit indirect_holder(const T& value_) : value(value_) { }
    T value;
    indirect_holder* next_;    // Intrusive link for hazard pointers

    indirect_holder* get_next() {
      return next_;
    }

    void set_next(indirect_holder* next) {
      next_ = next;
    }

    void destroy() {
      allocator::destroy(this);
    }
  };

 public:

  using value_type = T;
  using version_type = std::size_t;
  using marked_indirect_ptr = marked_ptr<indirect_holder>;

  using allocator = type_allocator<indirect_holder>;

  big_atomic() : version(0), indirect_value(allocator::create(T{})), fast_value{} {
    static_assert(std::is_default_constructible_v<T>);

    // force correct static initialization order
    allocator().init();
    hazptr_instance();

    T t{};
    std::memcpy(&fast_value, &t, sizeof(T));
  }

  /* implicit */ big_atomic(const T& t) : version(0), indirect_value(allocator::create(t)), fast_value{} {  // NOLINT(google-explicit-constructor)

    // force correct static initialization order
    allocator().init();
    hazptr_instance();

    std::memcpy(&fast_value, &t, sizeof(T));
  }

  T load() {
    //return fast_value;
    //return *reinterpret_cast<const T*>(&fast_value);
    //return std::bit_cast<T>(fast_value);
    //return internal::bits_to_object<T>(fast_value);

    auto ver = version.load(std::memory_order_acquire);
    alignas(T) char buffer[sizeof(T)];
    internal::atomic_load_per_byte_memcpy(&buffer, &fast_value, sizeof(T));
    //value_type v = internal::bits_to_object<T>(buffer);
    auto p = indirect_value.load();
    //std::atomic_thread_fence(std::memory_order_acquire);
    //return internal::bits_to_object<T>(fast_value);
    //return internal::bits_to_object<T>(buffer);
    //return v;
    if (
        !is_marked(p) &&
	ver == version.load(std::memory_order_relaxed))
      return internal::bits_to_object<T>(buffer);


    //std::abort();

    //auto reader = read_fast();
    //if (reader.fast_valid) {
    //  return reader.get_fast();
    //}
    else {
      auto hazptr = hazptr_holder{};
      auto p = hazptr.protect(indirect_value);
      assert(unmark_ptr(p) != nullptr);
      // Readers can help put the object back into "fast mode".  This is very good if updates
      // are infrequent since this will speed up all subsequent reads.  Unfortunately this
      // could be a severe slowdown if updates are very frequent, since many reads will try
      // to help and waste work here...  Might be nice to optimize this so that it doesn't
      // try to help every time, but only every once in a while.
      //if (p.get_mark() == SLOW_MODE) {
      //  try_seqlock_and_store(reader.num, p->value, p);
      //}
      return unmark_ptr(p)->value;
    }
    
  }

  void store(const T& desired) {
    auto num = version.load(std::memory_order_acquire);
    auto new_p = mark_ptr(allocator::create(desired));
    auto old_p = indirect_value.exchange(new_p);
    retire(unmark_ptr(old_p));
    try_seqlock_and_store(num, desired, new_p);
  }

  bool cas(const T& expected, const T& desired) {
    if (Equal{}(expected, desired)) {
      return load() == expected;
    }

    auto num = version.load(std::memory_order_acquire);

    // Can't use a fast read here -- we *need* to take a hazard pointer even if
    // the fast value is valid because otherwise our CAS below could ABA!!
    auto hazptr = hazptr_holder{};
    auto p = hazptr.protect(indirect_value);
    assert(unmark_ptr(p) != nullptr);

    if (!Equal{}(unmark_ptr(p)->value, expected)) {
      return false;
    }

    auto new_p = mark_ptr(allocator::create(desired));
    auto old_p = p;
    
    if ((indirect_value.load(std::memory_order_relaxed) == p && indirect_value.compare_exchange_strong(p, new_p))
         || (p == indirect_value.load(std::memory_order_relaxed) && p == unmark_ptr(old_p) && indirect_value.compare_exchange_strong(p, new_p))) {
           
      retire(unmark_ptr(p));
      try_seqlock_and_store(num, desired, new_p);
      return true;
    }
    else {
      allocator::destroy(unmark_ptr(new_p));
      return false;
    }
  }

  ~big_atomic() {
    auto p = unmark_ptr(indirect_value.load(std::memory_order_acquire));
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
    fast_reader reader;
    reader.num = version.load(std::memory_order_acquire);
    internal::atomic_load_per_byte_memcpy(&reader.buffer, &fast_value, sizeof(T));
    reader.p = indirect_value.load();
    assert(reader.p != nullptr);
    reader.fast_valid = reader.p.get_mark() != SLOW_MODE && reader.num == version.load(std::memory_order_relaxed);
    return reader;
  }

  void try_seqlock_and_store(version_type num, const T& desired, indirect_holder* p) noexcept {
    if ((num % 2 == 0) && num == version.load(std::memory_order_relaxed) && version.compare_exchange_strong(num, num + 1)) {
      internal::atomic_store_per_byte_memcpy(&fast_value, &desired, sizeof(T));
      version.store(num + 2, std::memory_order_release);
      indirect_value.compare_exchange_strong(p, unmark_ptr(p));
    }
  }

  struct hazptr_holder {
    indirect_holder* protect(const std::atomic<indirect_holder*>& src) const {
      return hazptr_instance().protect(src);
      //return src.load(std::memory_order_acquire);
    }
    
    ~hazptr_holder() {
      //hazptr_instance().release();
    }
  };

  static void retire(indirect_holder* p) {
    if (p) {
      hazptr_instance().retire(p);
    }
  }

  static HazardPointers<indirect_holder>& hazptr_instance() {
    return get_hazard_list<indirect_holder>();
  }

  static constexpr uintptr_t SLOW_MODE = 1;

  static constexpr indirect_holder* mark_ptr(indirect_holder* p) { return reinterpret_cast<indirect_holder*>(reinterpret_cast<uintptr_t>(p) | SLOW_MODE); }
  static constexpr indirect_holder* unmark_ptr(indirect_holder* p) { return reinterpret_cast<indirect_holder*>(reinterpret_cast<uintptr_t>(p) & ~SLOW_MODE); }
  static constexpr bool is_marked(indirect_holder* p) { return reinterpret_cast<uintptr_t>(p) & SLOW_MODE; }

  std::atomic<indirect_holder*> indirect_value{nullptr};
  std::atomic<version_type> version;
  alignas(16) char fast_value[sizeof(T)];  // Over-align in case 16-byte copies can use faster instructions (e.g., movdqa instead of movdqu on x86)
  //T fast_value;
};


}  // namespace parlay
