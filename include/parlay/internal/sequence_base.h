// This base class handles storage layout and memory allocation for sequences.
//
// It also handles whether the sequence is big or small (small size optimized).
// Conversion from small to large sequences and all allocations are handled here.
// This allows the higher-level logic of the sequence implementation to be written
// agnostic to these details, while this class handles the low-level stuff.

#ifndef PARLAY_INTERNAL_SEQUENCE_BASE_H
#define PARLAY_INTERNAL_SEQUENCE_BASE_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <array>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "../parallel.h"
#include "../portability.h"
#include "../type_traits.h"      // IWYU pragma: keep  // for is_trivially_relocatable
#include "../utilities.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "debug_uninitialized.h"
#endif

namespace parlay {
namespace sequence_internal {

// Sequence base class that handles storage layout and memory allocation
//
// Template arguments:
//  T = element type of the sequence
//  Allocator = An allocator for elements of type T
//  EnableSSO = True to enable small-size optimization
template<typename T, typename Allocator, bool EnableSSO>
struct alignas(uint64_t) sequence_base {

  // Only use SSO for trivial types
  constexpr static bool use_sso = EnableSSO && std::is_trivial<T>::value;

  // The maximum length of a sequence is 2^48 - 1.
  constexpr static uint64_t _max_size = (1LL << 48LL) - 1LL;

  using allocator_type = Allocator;
  using T_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;
  using raw_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<std::byte>;

  using value_type = T;

  // For trivial types, we want to specify a loop granularity so that (a) time isn't wasted
  // by the automatic granularity control on very small sequences, and (b) to give the optimizer
  // a higher chance to combine adjacent loop iterations (e.g. by converting copies in a loop
  // into a memcpy)

  static constexpr size_t copy_granularity(size_t) {
    return (std::is_trivially_copyable_v<value_type>) ? (1 + (1024 * sizeof(size_t) / sizeof(T))) : 0;
  }

  static constexpr size_t initialization_granularity(size_t) {
    return (std::is_trivially_default_constructible_v<value_type>) ? (1 + (1024 * sizeof(size_t) / sizeof(T))) : 0;
  }

  // This class handles internal memory allocation and whether
  // the sequence is big or small. We inherit from Allocator to
  // employ empty base optimization so that the size of the
  // sequence type is not increased when the allocator is stateless.
  // The memory layout is organized roughly as follows:
  //
  // struct {
  //
  //   union {
  //     // Long sequence
  //     struct {
  //       void*            buffer     --->   size_t        capacity
  //       uint64_t         n : 48            value_type[]  elements
  //     }
  //     // Short sequence
  //     struct {
  //       unsigned char    buffer[15]
  //     }
  //   }
  //
  //   // Only included when EnableSSO is true
  //   uint8_t              small_n : 7
  //   uint8_t              flag    : 1
  // }
  //
  // The union contains either a long sequence, which is a pointer
  // to a heap-allocated buffer prepended with its capacity, and a
  // 48-bit integer that stores the current size of the sequence.
  // Alternatively, it contains a short sequence, which is 15 bytes
  // of inline memory used to store elements of type T.
  //
  // If SSO is enabled, the 1 bit flag indicates whether the sequence
  // is long (1) or short (0). If the sequence is short, its size is
  // stored in the 7-bit size variable. This means that a zero-initialized
  // object represents a valid empty sequence. Short size optimization is
  // only enabled for trivial types, which means that a sequence is trivially
  // movable (i.e. you can move it by copying its raw bytes and zeroing
  // out the old one).
  //
  // On compilers / platforms that support the GNU C packed struct
  // extension, all of this fits into 16 bytes. This means that sequences
  // can be compare-exchanged (CAS'ed) on platforms that support a
  // 16-byte CAS operation. On other compilers / platforms, a sequence
  // might be 24 bytes if EnableSSO is true.
  //
  struct storage_impl : public T_allocator_type {

    // Empty constructor
    storage_impl() : T_allocator_type(), _data() { set_to_empty_representation(); }

    // Copy constructor
    storage_impl(const storage_impl& other) : T_allocator_type(other) {
      if (other.is_small()) {
        std::memcpy(static_cast<void*>(std::addressof(_data)), static_cast<const void*>(std::addressof(other._data)),
                    sizeof(_data));
      } else {
        auto n = other.size();
        initialize_capacity(n);
        auto buffer = data();
        auto other_buffer = other.data();
        parallel_for(
            0, n, [&](size_t i) { initialize_explicit(buffer + i, other_buffer[i]); }, copy_granularity(n));
        set_size(n);
      }
    }

    // Move constructor
    storage_impl(storage_impl&& other) noexcept { move_from(std::move(other)); }

    // Move assignment
    storage_impl& operator=(storage_impl&& other) noexcept {
      clear();
      move_from(std::move(other));
      return *this;
    }

    // Allocator access

    T_allocator_type& get_T_allocator() { return *static_cast<T_allocator_type*>(this); }

    const T_allocator_type& get_T_allocator() const { return *static_cast<const T_allocator_type*>(this); }

    allocator_type get_allocator() const { return allocator_type(get_T_allocator()); }

    raw_allocator_type get_raw_allocator() const { return raw_allocator_type(get_T_allocator()); }

    // Swap a sequence backend with another. Since small sequences
    // must contain trivial types, a sequence can always be swapped
    // by swapping raw bytes.
    void swap(storage_impl& other) {
      // Swap raw bytes. Every linter complains about this so
      // we need a lot of warning suppressions, unfortunately
      std::byte tmp[sizeof(*this)];
      // cppcheck-suppress memsetClass
      std::memcpy(std::addressof(tmp), static_cast<void*>(this), sizeof(*this));    // NOLINT
      // cppcheck-suppress memsetClass
      std::memcpy(static_cast<void*>(this), &other, sizeof(*this));                 // NOLINT
      // cppcheck-suppress memsetClass
      std::memcpy(static_cast<void*>(std::addressof(other)), &tmp, sizeof(*this));  // NOLINT
    }

    // Destroy a sequence backend
    ~storage_impl() { clear(); }

    // Move the contents of other into this sequence
    // Assumes that this sequence is empty. Callers
    // should call clear() before calling this function.
    void move_from(storage_impl&& other) {
      // Since small sequences contain trivial types, moving
      // them just means copying their raw bytes, and zeroing
      // out the old sequence. For large sequences, this will
      // copy their buffer pointer and size.

      // cppcheck-suppress memsetClass
      std::memcpy(static_cast<void*>(this), &other, sizeof(*this));  // NOLINT

      other.set_to_empty_representation();
    }

    // Call the destructor on all elements
    void destroy_all() {
      if (!std::is_trivially_destructible<value_type>::value) {
        auto n = size();
        auto current_buffer = data();
        parallel_for(0, n, [&](size_t i) { destroy(&current_buffer[i]); });
      }
    }

    // Destroy all elements, free the buffer (if any)
    // and set the sequence to the empty sequence
    void clear() {
      // Small sequences can only hold trivial
      // types, so no destruction is necessary
      if (!is_small()) {
        destroy_all();
        auto alloc = get_raw_allocator();
        _data.long_mode.buffer.free_buffer(alloc);
      }

      set_to_empty_representation();
    }

    // Free the buffer without destroying the elements
    // of the sequence. This is intended for use after
    // the contents of the sequence are relocated
    void clear_without_destruction() {
      // Small sequences can only hold trivial
      // types, so no destruction is necessary
      if (!is_small()) {
        auto alloc = get_raw_allocator();
        _data.long_mode.buffer.free_buffer(alloc);
      }

      set_to_empty_representation();
    }

// GCC-8+ is unhappy with memsetting the object to zero
#if defined(__GNUC__) && __GNUC__ >= 8 && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

    // A zero-byte-filled sequence always corresponds
    // to an empty sequence, whether small or large
    void set_to_empty_representation() {
      // cppcheck-suppress memsetClass
      std::memset(this, 0, sizeof(*this));  // NOLINT
    }

#if defined(__GNUC__) && __GNUC__ >= 8 && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// GCC gives a lot of false positive diagnoses for
// "maybe uninitialized" variables inside long_seq
// and capacitated_buffer so we disable that check
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

    // A buffer of value_types with a size_t prepended
    // to the front to store the capacity. Does not
    // handle construction or destruction or the
    // elements in the buffer.
    //
    // IMPORTANT: Memory is not freed on destruction since it
    // requires a reference to the allocator to do so, so
    // free_buffer(alloc) must be called manually before the
    // buffer is destroyed!
    struct capacitated_buffer {

      // Prepended header buffer. This is the magic type that makes
      // this all work. It works by constructing a buffer of size
      //
      //   offsetof(header, data) + capacity * sizeof(value_type)
      //
      // in order to get an array of value_types of size capacity.
      //
      // How/why does this work? First a disclaimer: This is still almost
      // certainly undefined behaviour, but until C++ gets flexible array members
      // this is probably the best we can do. The trick we use is to have a
      // single std::byte data member that is aligned to the alignment of a
      // value_type object to tell us how far away from capacity we should put
      // the data. This prevents alignment issues where the data might get too
      // close to the capacity. With this member, we can use offsetof to compute
      // its position, at which we then allocate space for capacity elements
      // of type value_type.
      struct header {
        const size_t capacity;
        union {
          alignas(value_type) std::byte data[1];
        };

        static header* create(size_t capacity, raw_allocator_type& a) {
          auto buffer_size = offsetof(header, data) + capacity * sizeof(value_type);
          std::byte* bytes = std::allocator_traits<raw_allocator_type>::allocate(a, buffer_size);
          return new (bytes) header(capacity);
        }

        static void destroy(header* p, raw_allocator_type& a) {
          auto buffer_size = offsetof(header, data) + p->capacity * sizeof(value_type);
          std::byte* bytes = reinterpret_cast<std::byte*>(p);
          std::allocator_traits<raw_allocator_type>::deallocate(a, bytes, buffer_size);
        }

        explicit header(size_t _capacity) : capacity(_capacity) {}
        ~header() = delete;

        value_type* get_data() { return reinterpret_cast<value_type*>(std::addressof(data)); }
        const value_type* get_data() const { return reinterpret_cast<const value_type*>(std::addressof(data)); }
      };

      // Construct a capacitated buffer with the capacity to hold the given
      // number of objects of type value_type, using the given allocator
      capacitated_buffer(size_t capacity, raw_allocator_type& a) : buffer(header::create(capacity, a)) {
        assert(buffer != nullptr);
        assert(get_capacity() == capacity);
      }

      // This unfortunately can not go in the destructor and be done
      // automatically because we need a reference to the allocator
      // to deallocate the buffer, and we do not want to store the
      // allocator here.
      void free_buffer(raw_allocator_type& a) {
        if (buffer != nullptr) {
          header::destroy(buffer, a);
          buffer = nullptr;
        }
      }

      size_t get_capacity() const {
        if (buffer != nullptr) {
          return buffer->capacity;
        } else {
          return 0;
        }
      }

      value_type* data() {
        if (buffer != nullptr) {
          return buffer->get_data();
        } else {
          return nullptr;
        }
      }

      const value_type* data() const {
        if (buffer != nullptr) {
          return buffer->get_data();
        } else {
          return nullptr;
        }
      }

      header* buffer;
    } PARLAY_PACKED;

    // A not-short-size-optimized sequence. Elements are
    // stored in a heap-allocated buffer. We use a 48-bit
    // integer to store the size so that there is room left
    // over for the flag to discriminate between small and
    // large sequences.

    // This requires the GNU C packed struct extension which
    // might not always be available, in which case this object
    // will be 16 bytes, making the entire sequence 24 bytes.
    struct long_seq {
      capacitated_buffer buffer;
      uint64_t n : 48;

      long_seq(capacitated_buffer _buffer, uint64_t _n) : buffer(_buffer), n(_n) {}
      ~long_seq() = default;

      void set_size(size_t new_size) { n = new_size; }

      size_t get_size() const { return n; }

      size_t capacity() const { return buffer.get_capacity(); }

      value_type* data() { return buffer.data(); }

      const value_type* data() const { return buffer.data(); }
    } PARLAY_PACKED;

    // The maximum capacity of a short-size-optimized sequence
    constexpr static size_t short_capacity = (sizeof(capacitated_buffer) + sizeof(uint64_t) - 1) / sizeof(value_type);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

    // A short-size-optimized sequence. Elements are stored
    // inline in the data structure.
    struct short_seq {
      std::array<value_type, short_capacity> elements;

      short_seq() = delete;
      ~short_seq() = delete;

      value_type* data() { return elements.data(); }
      const value_type* data() const { return elements.data(); }
    } PARLAY_PACKED;


    // Store either a short or a long sequence. By default, we
    // store an empty short sequence, which can be represented
    // by a zero-initialized object.
    //
    // Flag is used to discriminate which type is currently stored
    // by the union. 1 corresponds to a long sequence, 0 to a short
    // sequence.
    struct _data_impl {
      _data_impl() {}
      ~_data_impl(){};

      union {
        typename std::conditional<use_sso, short_seq, void*>::type short_mode;
        long_seq long_mode;
      } PARLAY_PACKED;

      uint8_t small_n : 7;
      uint8_t flag : 1;
    } _data;

    // Returns true if the sequence is in small size mode
    constexpr bool is_small() const {
      if constexpr (use_sso) {
        return _data.flag == 0;
      } else {
        return false;
      }
    }

    // Return the size of the sequence
    size_t size() const {
      if constexpr (use_sso) {
        if (is_small())
          return _data.small_n;
        else
          return _data.long_mode.get_size();
      } else {
        return _data.long_mode.get_size();
      }
    }

    // Return the capacity of the sequence
    size_t capacity() const {
      if (is_small())
        return short_capacity;
      else
        return _data.long_mode.capacity();
    }

    value_type* data() {
      if constexpr (use_sso) {
        if (is_small()) return _data.short_mode.data();
        else return _data.long_mode.data();
      }
      else {
        return _data.long_mode.data();
      }
    }

    const value_type* data() const {
      if constexpr (use_sso) {
        if (is_small()) return _data.short_mode.data();
        else return _data.long_mode.data();
      }
      else {
        return _data.long_mode.data();
      }
    }

    void set_size(size_t new_size) {
      assert(new_size <= capacity());
      if constexpr (use_sso) {
        if (is_small())
          _data.small_n = new_size;
        else
          _data.long_mode.set_size(new_size);
      } else {
        _data.long_mode.set_size(new_size);
      }
    }

    // Constructs an object of type value_type at an
    // uninitialized memory location p using args...
    template<typename... Args>
    void initialize(value_type* p, Args&&... args) {
      std::allocator_traits<T_allocator_type>::construct(*this, p, std::forward<Args>(args)...);
    }

    // Constructs an object of type value_type at an uninitialized
    // memory location p using f() by copy elision. This circumvents
    // the allocator and hence should only be used when initialize
    // and initialize_explicit are not applicable (e.g., for a type
    // that is not copyable or movable).
    template<typename F>
    void initialize_with_copy_elision(value_type* p, F&& f) {
      static_assert(std::is_same_v<value_type, std::invoke_result_t<F&&>>);
      new (p) value_type(f());
    }

    // Perform a copy or move initialization. This is equivalent
    // to initialize(forward(v)) above, except that it will not
    // allow accidental implicit conversions. This prevents someone
    // from, for example, attempting to append {1,2,3} to a
    // sequence of vectors, and ending up with 3 new vectors
    // of length 1,2,3 respectively.

    void initialize_explicit(value_type* p, const value_type& v) {
      std::allocator_traits<T_allocator_type>::construct(*this, p, v);
    }

    void initialize_explicit(value_type* p, value_type&& v) {
      std::allocator_traits<T_allocator_type>::construct(*this, p, std::move(v));
    }

    // Destroy the object of type value_type pointed to by p
    void destroy(value_type* p) { std::allocator_traits<T_allocator_type>::destroy(*this, p); }

    // Return a reference to the i'th element
    value_type& at(size_t i) { return data()[i]; }

    const value_type& at(size_t i) const { return data()[i]; }

    // Should only be called during initialization. Same as
    // ensure_capacity, except does not need to copy elements
    // from the existing buffer.
    void initialize_capacity(size_t desired) {
      if constexpr (use_sso) {
        // Initialize the sequence in large mode
        if (short_capacity < desired) {
          auto alloc = get_raw_allocator();
          _data.flag = 1;
          _data.long_mode = long_seq(capacitated_buffer(desired, alloc), 0);
        } else {
          _data.flag = 0;
        }
      } else {
        auto alloc = get_raw_allocator();
        _data.long_mode = long_seq(capacitated_buffer(desired, alloc), 0);
      }
      assert(capacity() >= desired);
    }

    // Ensure that the capacity is at least new_capacity. The
    // actual capacity may be increased to a larger amount.
    void ensure_capacity(size_t desired) {
      auto current = capacity();

      // Allocate a new buffer and move the current
      // contents of the sequence into the new buffer
      if (current < desired) {
        // Allocate a new buffer that is at least
        // 50% larger than the old capacity
        size_t new_capacity = (std::max)(desired, (5 * current)/ 2);//(15 * current + 9) / 10);
        auto alloc = get_raw_allocator();
        capacitated_buffer new_buffer(new_capacity, alloc);

        // If uninitialized debugging is enabled, mark the new memory as uninitialized
#ifdef PARLAY_DEBUG_UNINITIALIZED
        if constexpr (std::is_same_v<value_type, internal::UninitializedTracker>) {
          auto buffer = new_buffer.data();
          parallel_for(0, new_buffer.get_capacity(), [&](size_t i) {
            buffer[i].initialized = false;
            PARLAY_ASSERT_UNINITIALIZED(buffer[i]);
          });
        }
#endif

        // Move initialize the new buffer with the
        // contents of the old buffer
        auto n = size();
        auto dest_buffer = new_buffer.data();
        auto current_buffer = data();
        uninitialized_relocate_n_a(dest_buffer, current_buffer, n, *this);

        // Destroy the old stuff
        if (!is_small()) {
          _data.long_mode.buffer.free_buffer(alloc);
        }

        // Assign the new stuff
        if constexpr (use_sso) {
          _data.flag = 1;  // mark as a large sequence
        }
        _data.long_mode = long_seq(new_buffer, n);
      }

      assert(capacity() >= desired);
    }
  };

  sequence_base() : storage() {}
  explicit sequence_base(const storage_impl& other) : storage(other) {}
  explicit sequence_base(storage_impl&& other) : storage(std::move(other)) {}
  ~sequence_base() {}

  storage_impl storage;
};

}  // namespace sequence_internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_SEQUENCE_BASE_H
