// A sequence is a dynamic array supporting parallel
// modification operations. It can be thought of as a
// parallel version of std::vector.
//
// It has the following features:
// - Parallel modification operations
// - It is 16 bytes**, so it can be compare-exchanged (CAS'd)
//   if a 16-byte CAS operation is available.
// - It supports small-size optimization (SSO).
//   * For sequences whose elements fit in <= 15 bytes, they
//     will be stored inline with no heap allocation
//   * SSO is only applied for trivial types, to ensure
//     that small sequences are still CASable.
// - It supports the interface of std::vector, so it can be
//   used as a parallel drop-in replacement
// - It supports custom memory allocators
// - Supports sequences of size up to 2^48 - 1
//
// ** parlay::sequence is only 16 bytes with compilers / platforms
// that support GNU C's packed struct extension, and when the allocator
// is stateless. On other compilers or platforms, parlay::sequence might
// be 24 bytes, plus the size of the allocator.

#ifndef PARLAY_SEQUENCE_H_
#define PARLAY_SEQUENCE_H_

#include <cassert>
#include <cstdint>
#include <cstring>

#include <initializer_list>
#include <iterator>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "alloc.h"
#include "parallel.h"
#include "range.h"
#include "slice.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "internal/debug_uninitialized.h"
#endif  // PARLAY_DEBUG_UNINITIALIZED

namespace parlay {

#ifndef PARLAY_USE_STD_ALLOC
template<typename T>
using _sequence_default_allocator = parlay::allocator<T>;
#else
template<typename T>
using _sequence_default_allocator = std::allocator<T>;
#endif

// This base class handles memory allocation for sequences.
//
// It also handles whether the sequence is big or small
// (small size optimized). Conversion from small to large and
// all allocations are handled here. The main sequence class
// handles the higher level logic.
template <typename T, typename Allocator>
struct _sequence_base {

  // The maximum length of a sequence is 2^48 - 1.
  constexpr static uint64_t _max_size = (1LL << 48LL) - 1LL;

  using allocator_type = Allocator;
  using byte_type = unsigned char;
  using value_type = T;

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
  //   uint8_t              small_n : 7
  //   uint8_t              flag    : 1
  // }
  //
  // The union contains either a long sequence, which is a pointer
  // to a heap-allocated buffer prepended with its capacity, and a
  // 48-bit integer that stores the current size of the sequence.
  // Alternatively, it contains a short sequence, which is 15 bytes
  // of inline memory used to store elements of type T. The 1 bit
  // flag indicates whether the sequence is long (1) or short (0).
  // If the sequence is short, its size is stored in the 7-bit size
  // variable. This means that a zero-initialized object represents
  // a valid empty sequence. Short size optimization is only enabled
  // for trivial types, which means that a sequence is trivially
  // movable (i.e. you can move it by copying its raw bytes and zeroing
  // out the old one).
  //
  // On compilers / platforms that support the GNU C packed struct
  // extension, all of this fits into 16 bytes. This means that sequences
  // can be compare-exchanged (CAS'ed) on platforms that support a
  // 16-byte CAS operation. On other compilers / platforms, a sequence
  // might be 24 bytes.
  //
  struct _sequence_impl : public allocator_type {

    // Empty constructor
    _sequence_impl() : allocator_type(), _data() { }

    // Copy constructor
    _sequence_impl(const _sequence_impl& other) : allocator_type(other) {
      auto n = other.size();
      ensure_capacity(n);
      auto buffer = data();
      auto other_buffer = other.data();
      parallel_for(0, n, [&](size_t i) {
        initialize_explicit(buffer + i, other_buffer[i]);
      });
      set_size(n);
    }

    // Move constructor
    _sequence_impl(_sequence_impl&& other) noexcept {
      move_from(std::move(other));
    }

    // Move assignment
    _sequence_impl& operator=(_sequence_impl&& other) noexcept {
      clear();
      move_from(std::move(other));
      return *this;
    }

    // Swap a sequence backend with another. Since small sequences
    // must contain trivial types, a sequence can always be swapped
    // by swapping raw bytes.
    void swap(_sequence_impl& other) {
      // Swap raw bytes
      byte_type tmp[sizeof(*this)];
      std::memcpy(&tmp, this, sizeof(*this));           // NOLINT
      std::memcpy(this, &other, sizeof(*this));         // NOLINT
      std::memcpy(&other, &tmp, sizeof(*this));         // NOLINT
    }

    // Destroy a sequence backend
    ~_sequence_impl() {
      clear();
    }

    // Move the contents of other into this sequence
    // Assumes that this sequence is empty. Callers
    // should call clear() before calling this function.
    void move_from(_sequence_impl&& other) {
      assert(size() == 0);
      // Since small sequences contain trivial types,
      // moving them just means copying their raw bytes,
      // and zeroing out the old sequence. For large
      // sequences, this will copy their buffer pointer
      // and size.
      std::memcpy(this, &other, sizeof(*this));         // NOLINT
      
      other._data.flag = 0;
      other._data.small_n = 0;
    }

    // Call the destructor on all elements
    void destroy_all() {
      if (!std::is_trivially_destructible<value_type>::value) {
        auto n = size();
        auto current_buffer = data();
        parallel_for(0, n, [&](size_t i) {
          destroy(&current_buffer[i]);
        });
      }
    }

    // Destroy all elements, free the buffer (if any)
    // and set the sequence to the empty sequence
    void clear() {
      // Small sequences can only hold trivial
      // types, so no destruction is necessary
      if (!is_small()) {
        destroy_all();
        _data.long_mode.buffer.free_buffer(*this);
      }
      
      _data.flag = 0;
      _data.small_n = 0;
    }

    // A buffer of value_types with a size_t prepended
    // to the front to store the capacity. Does not 
    // handle construction or destruction or the
    // elements in the buffer.
    //
    // Memory is not freed on destruction since it requires
    // the allocator to do so, so free_buffer(alloc) must be
    // called manually before the buffer is destroyed!
    struct capacitated_buffer {

      // It takes "offset" objects of (value_type) to have enough
      // memory to store a size_t. We use this to prepend the buffer
      // with its capacity.
      constexpr static size_t offset =
        (sizeof(size_t) + sizeof(value_type) - 1) / sizeof(value_type);

      capacitated_buffer(size_t capacity, allocator_type& a)
        : buffer(a.allocate(capacity + offset)) {
          assert(buffer != nullptr);
          set_capacity(capacity);
        }

      void free_buffer(allocator_type& a) {
        assert(buffer != nullptr);
        auto sz = get_capacity() + offset;
        a.deallocate(static_cast<value_type*>(buffer), sz);
        buffer = nullptr;
      }

      void set_capacity(size_t capacity) {
        *reinterpret_cast<size_t*>(buffer) = capacity;
        assert(get_capacity() == capacity);
      }

      size_t get_capacity() const {
        return *reinterpret_cast<size_t*>(buffer);
      }

      value_type* data() {
        return static_cast<value_type*>(buffer) + offset;
      }

      const value_type* data() const {
        return static_cast<value_type*>(buffer) + offset;
      }

      void* buffer;
    }
#if defined(__GNUC__)
    __attribute__((packed))
#endif
    ;

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
      
      long_seq() = delete;
      ~long_seq() = delete;
      
      void set_size(size_t new_size) {
        n = new_size;
      }
      
      size_t get_size() const {
        return n;
      }
      
      size_t capacity() const {
        return buffer.get_capacity();
      }
      
      value_type* data() {
        return buffer.data();
      }
      
      const value_type* data() const {
        return buffer.data();
      }
    }
#if defined(__GNUC__)
    // Pack this struct into 14 bytes if possible
    __attribute__((packed))
#endif
    ;

    // A short-size-optimized sequence. Elements are stored
    // inline in the data structure.
    struct short_seq {
      byte_type buffer[15];
      
      short_seq() = delete;
      ~short_seq() = delete;
      
      size_t capacity() const {
        // The following check prevents the use of small-size
        // optimization for non-trivial types. This is important
        // because we want small-size optimized sequences to be
        // trivially copyable/movable.
        if (std::is_trivial<value_type>::value) {
          return (sizeof(long_seq) - 1) / sizeof(value_type);
        }
        else {
          return 0;
        }
      }
      
      value_type* data() {
        return (value_type*)&buffer;
      }
      
      const value_type* data() const {
        return (value_type*)&buffer;
      }
    };

    // Store either a short or a long sequence. By default, we
    // store an empty short sequence, which can be represented
    // by a zero-initialized object.
    //
    // Flag is used to discriminate which type is currently stored
    // by the union. 1 corresponds to a long sequence, 0 to a short
    // sequence.
    struct _data_impl {
      _data_impl() : small_n(0), flag(0) { }
      ~_data_impl() { };
      
      union {
        short_seq short_mode;
        long_seq long_mode;
      };
      
      uint8_t small_n : 7;
      uint8_t flag : 1;
    } _data;

    // Returns true if the sequence is in small size mode
    bool is_small() const {
      return _data.flag == 0;
    }

    // Return the size of the sequence
    size_t size() const {
      if (is_small()) return _data.small_n;
      else return _data.long_mode.get_size();
    }

    // Return the capacity of the sequence
    size_t capacity() const {
      if (is_small()) return _data.short_mode.capacity();
      else return _data.long_mode.capacity();
    }

    value_type* data() {
      if (is_small()) return _data.short_mode.data();
      else return _data.long_mode.data();
    }
    
    const value_type* data() const {
      if (is_small()) return _data.short_mode.data();
      else return _data.long_mode.data();
    }

    void set_size(size_t new_size) {
      assert(new_size <= capacity());
      if (is_small()) _data.small_n = new_size;
      else _data.long_mode.set_size(new_size);
    }
    
    // Constructs am object of type value_type at an
    // uninitialized memory location p using args...
    template<typename... Args>
    void initialize(value_type* p, Args&&... args) {
      std::allocator_traits<allocator_type>::construct(
        *this, p, std::forward<Args>(args)...);
    }
    
    // Perform a copy or move initialization. This is equivalent
    // to initialize(forward(v)) above, except that it will not
    // allow accidental implicit conversions. This prevents someone
    // from, for example, attempting to append {1,2,3} to a 
    // sequence of vectors, and ending up with 3 new vectors
    // of length 1,2,3 respectively.
    
    void initialize_explicit(value_type* p, const value_type& v) {
      std::allocator_traits<allocator_type>::construct(
        *this, p, v);
    }
    
    void initialize_explicit(value_type* p, value_type&& v) {
      std::allocator_traits<allocator_type>::construct(
        *this, p, std::move(v));
    }
    
    // Destroy the object of type value_type pointed to by p
    void destroy(value_type* p) {
      std::allocator_traits<allocator_type>::destroy(*this, p);
    }

    // Return a reference to the i'th element
    value_type& at(size_t i) {
      return data()[i];
    }
    
    const value_type& at(size_t i) const {
      return data()[i];
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
        size_t new_capacity = (std::max)(desired,
          (15 * current + 9) / 10);
        capacitated_buffer new_buffer(new_capacity, *this);
        
        // Move initialize the new buffer with the
        // contents of the old buffer
        auto n = size();
        auto dest_buffer = new_buffer.data();
        auto current_buffer = data();
        parallel_for(0, n, [&](size_t i) {
          initialize_explicit(dest_buffer + i, std::move(current_buffer[i]));
          current_buffer[i].~value_type();
        });
        
        // Destroy the old stuff
        if (!is_small()) {
          _data.long_mode.buffer.free_buffer(*this);
        }
        
        // Assign the new stuff
        _data.flag = 1;  // large sequence
        _data.long_mode.buffer = new_buffer;
        _data.long_mode.set_size(n);
      }
      
      assert(capacity() >= desired);
    }
  };

  _sequence_base() : impl() { }
  explicit _sequence_base(const _sequence_impl& other) : impl(other) { }
  explicit _sequence_base(_sequence_impl&& other) : impl(std::move(other)) { }
  ~_sequence_base() { }

  _sequence_impl impl;
};

// A sequence is a dynamic contiguous container that operates
// in parallel. It supports parallel construction, resizing,
// destruction, bulk insertion, bulk erasure, etc. Essentially,
// it is a parallel version of std::vector.
template <typename T, typename Allocator = _sequence_default_allocator<T>>
class sequence : protected _sequence_base<T, Allocator> {
  
  // Ensure that T is not const or volatile
  static_assert(std::is_same<typename std::remove_cv<T>::type, T>::value,
    "parlay::sequence must have a non-const, non-volatile value_type");
  
 public:

  // --------------- Container requirements ---------------

  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using pointer = T*;
  using const_pointer = const T*;
  
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  
  using view_type = slice<iterator, iterator>;
  using const_view_type = slice<const_iterator, const_iterator>;
  
  using _sequence_base<T, Allocator>::impl;
  using _sequence_base<T, Allocator>::_max_size;

  // creates an empty sequence
  sequence() : _sequence_base<T, Allocator>() { }

  // creates a copy of s
  sequence(const sequence<T, Allocator>& s)
    : _sequence_base<T, Allocator>(s.impl) { }

  // moves rv
  sequence(sequence<T, Allocator>&& rv) noexcept
    : _sequence_base<T, Allocator>(std::move(rv.impl)) { }

  // copy and move assignment
  sequence<T, Allocator>& operator=(sequence<T, Allocator> b) {
    swap(b);
    return *this;
  }

  // destroys all elements and frees all memory
  ~sequence() { };

  iterator begin() { return impl.data(); }
  iterator end() { return impl.data() + impl.size(); }

  const_iterator begin() const { return impl.data(); }
  const_iterator end() const { return impl.data() + impl.size(); }

  const_iterator cbegin() const { return impl.data(); }
  const_iterator cend() const { return impl.data() + impl.size(); }
  
  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
  
  const_reverse_iterator rbegin() const { return std::make_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return std::make_reverse_iterator(begin()); }
  
  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }

  bool operator==(const sequence<T, Allocator>& other) const {
    return size() == other.size() && compare_equal(other.begin());
  }

  bool operator!=(const sequence<T, Allocator>& b) const {
    return !(*this == b);
  }

  void swap(sequence<T, Allocator>& b) {
    impl.swap(b.impl); 
  }

  size_type size() const { return impl.size(); }

  size_type max_size() const {
    if (std::numeric_limits<size_type>::max() < _max_size) {
      return std::numeric_limits<size_type>::max();
    }
    else {
      return _max_size;
    }
  }

  bool empty() const { return size() == 0; }
  
  // -------------- Random access interface ----------------
  
  value_type* data() { return impl.data(); }
  
  const value_type* data() const { return impl.data(); }

  value_type& operator[](size_t i) { return impl.at(i); }
  const value_type& operator[](size_t i) const { return impl.at(i); }
  
  value_type& at(size_t i) {
    if (i >= size()) {
      throw std::out_of_range("sequence access out of bounds: length = " +
                              std::to_string(size()) + ", index = " +
                              std::to_string(i));
    }
    else {
      return impl.at(i);
    }
  }
  
  const value_type& at(size_t i) const {
    if (i >= size()) {
      throw std::out_of_range("sequence access out of bounds: length = " +
                              std::to_string(size()) + ", index = " +
                              std::to_string(i));
    }
    else {
      return impl.at(i);
    }
  }

  // ------------ SequenceContainer requirements ---------------

  // Construct a sequence of length n. Elements will be
  // value initialized
  explicit sequence(size_t n) : _sequence_base<T, Allocator>() {
    initialize_default(n);
  }

  // Constructs a sequence consisting of n copies of t
  sequence(size_t n, const value_type& t) : _sequence_base<T, Allocator>() {
    initialize_fill(n, t);
  }

  // Constructs a sequence consisting of the elements
  // in the given iterator range
  template<typename _Iterator>
  sequence(_Iterator i, _Iterator j) : _sequence_base<T, Allocator>() {
    initialize_dispatch(i, j, std::is_integral<_Iterator>());
  }

  // Constructs a sequence from the elements of the given initializer list
  //
  // Note: cppcheck flags all implicit constructors. This one is okay since
  // we want to convert initializer lists into sequences.
  sequence(std::initializer_list<value_type> l) : _sequence_base<T, Allocator>() {      // cppcheck-suppress noExplicitConstructor
    initialize_range(std::begin(l), std::end(l),
      typename std::iterator_traits<decltype(std::begin(l))>::iterator_category());
  }

  sequence<T, Allocator>& operator=(std::initializer_list<value_type> l) {
    impl.clear();
    initialize_range(std::begin(l), std::end(l),
      typename std::iterator_traits<decltype(std::begin(l))>::iterator_category());
    return *this;
  }

  template<typename... Args>
  iterator emplace(iterator p, Args&&... args) {
    if (p == end()) {
      return emplace_back(std::forward<Args>(args)...);
    }
    else {
      // p might be invalidated when the capacity is increased,
      // so we need to remember where it occurs in the sequence
      auto pos = p - begin();
      impl.ensure_capacity(size() + 1);
      p = begin() + pos;

      // Note that "it" is guaranteed to remain valid even after
      // the call to move_append since we ensured that there was
      // sufficient capacity already, so a second reallocation will
      // never happen after this point
      auto the_tail = pop_tail(p);
      auto it = emplace_back(std::forward<Args>(args)...);
      move_append(the_tail);
      return it;
    }
  }
  
  template<typename... Args>
  iterator emplace_back(Args&&... args) {
    impl.ensure_capacity(size() + 1);
    impl.initialize(end(), std::forward<Args>(args)...);
    impl.set_size(size() + 1);
    return end() - 1;
  }

  iterator push_back(const value_type& v) {
    return emplace_back(v);
  }
  
  iterator push_back(value_type&& v) {
    return emplace_back(std::move(v));
  }

  template<typename _Iterator>
  iterator append(_Iterator first, _Iterator last) {
    return append_dispatch(first, last, std::is_integral<_Iterator>());
  }
  
  template<typename R>
  iterator append(R&& r) {
    if constexpr (std::is_lvalue_reference<R>::value) {
      return append(std::begin(r), std::end(r));
    }
    else {
      return append(std::make_move_iterator(std::begin(r)),
                      std::make_move_iterator(std::end(r)));
    }
  }
  
  iterator append(size_t n, const value_type& t) {
    return append_n(n, t);
  }

  iterator insert(iterator p, const value_type& t) {
    return emplace(p, t); 
  }

  iterator insert(iterator p, value_type&& rv) {
    return emplace(p, std::move(rv));
  }

  iterator insert(iterator p, size_t n, const value_type& t) {
    return insert_n(p, n, t);
  }

  template<typename _Iterator>
  iterator insert(iterator p, _Iterator i, _Iterator j) {
    return insert_dispatch(p, i, j, std::is_integral<_Iterator>());
  }

  template<typename R>
  iterator insert(iterator p, R&& r) {
    if constexpr (std::is_lvalue_reference<R>::value) {
      return insert(p, std::begin(r), std::end(r));
    }
    else {
      return insert(p, std::make_move_iterator(std::begin(r)),
                      std::make_move_iterator(std::end(r)));
    }
  }

  iterator insert(iterator p, std::initializer_list<value_type> l) {
    return insert_range(p, std::begin(l), std::end(l));
  }

  iterator erase(iterator q) {
    if (q == end() - 1) {
      pop_back();
      return end();
    }
    else {
      auto pos = q - begin();
      auto the_tail = pop_tail(q+1);
      pop_back();
      move_append(the_tail);
      return begin() + pos;
    }
  }

  iterator erase(iterator q1, iterator q2) {
    auto pos = q1 - begin();
    auto the_tail = pop_tail(q2);
    resize(size() - (q2 - q1));
    move_append(the_tail);
    return begin() + pos;
  }
  
  void pop_back() {
    impl.destroy(end() - 1);
    impl.set_size(size() - 1);
  }

  void clear() { impl.clear(); }
  
  void resize(size_t new_size, const value_type& v = value_type()) {
    auto current = size();
    if (new_size < current) {
      auto buffer = impl.data();
      parallel_for(new_size, current, [&](size_t i) {
        impl.destroy(&buffer[i]);
      });
    }
    else {
      impl.ensure_capacity(new_size);
      assert(impl.capacity() >= new_size);
      auto buffer = impl.data();
      parallel_for(current, new_size, [&](size_t i) {
        impl.initialize_explicit(&buffer[i], v);
      });
    }
    impl.set_size(new_size);
  }

  void reserve(size_t amount) {
    impl.ensure_capacity(amount);
    assert(impl.capacity() >= amount);
  }
  
  size_t capacity() const {
    return impl.capacity();
  }

  template<typename _Iterator>
  void assign(_Iterator i, _Iterator j) {
    impl.clear();
    initialize_dispatch(i, j, std::is_integral<_Iterator>());
  }
  
  void assign(size_t n, const value_type& v) {
    impl.clear();
    return initialize_fill(n, v);
  }
  
  void assign(std::initializer_list<value_type> l) {
    assign(std::begin(l), std::end(l));
  }
  
  template<typename R>
  void assign(R&& r) {
    if constexpr (std::is_lvalue_reference<R>::value) {
      return assign(std::begin(r), std::end(r));
    }
    else {
      return assign(std::make_move_iterator(std::begin(r)),
              std::make_move_iterator(std::end(r)));
    }
    assign(std::begin(r), std::end(r));
  }

  value_type& front() { return *begin(); }
  value_type& back() { return *(end() - 1); }
  
  const value_type& front() const { return *begin(); }
  const value_type& back() const { return *(end() - 1); }
  
  auto head(iterator p) {
    return make_slice(begin(), p);
  }
  
  auto head(size_t len) {
    return make_slice(begin(), begin() + len);
  }

  auto tail(iterator p) {
    return make_slice(p, end());
  }
  
  auto tail(size_t len) {
    return make_slice(end() - len, end());
  }

  auto cut(size_t s, size_t e) {
    return make_slice(begin()+s, begin()+e);
  }

  // Const versions of slices
  
  auto head(iterator p) const {
    return make_slice(begin(), p);
  }
  
  auto head(size_t len) const {
    return make_slice(begin(), begin() + len);
  }

  auto tail(iterator p) const {
    return make_slice(p, end());
  }
  
  auto tail(size_t len) const {
    return make_slice(end() - len, end());
  }

  auto cut(size_t s, size_t e) const {
    return make_slice(begin()+s, begin()+e);
  }

  // Remove all elements of the subsequence beginning at the element
  // pointed to by p, and return a new sequence consisting of those
  // removed elements.
  sequence<value_type> pop_tail(iterator p) {
    if (p == end()) {
      return sequence<value_type>{};
    }
    else {
      sequence<value_type> the_tail(std::make_move_iterator(p),
        std::make_move_iterator(end()));
      parallel_for(0, end() - p, [&](size_t i) { impl.destroy(&p[i]); });
      impl.set_size(p - begin());
      return the_tail;
    }
  }
  
  // Remove the last len elements from the sequence and return
  // a new sequence consisting of the removed elements
  sequence<value_type> pop_tail(size_t len) {
    return pop_tail(end() - len);
  }

  // ----------------- Factory methods --------------------

  // Create a sequence of length n consisting of uninitialized
  // elements. This is potentially dangerous! Use at your own
  // risk. For primitive types, this is mostly harmless, since
  // the elements will essentially just be arbitrary. For
  // non-trivial types, you must ensure that you initialize
  // every element of the sequence before invoking any operation
  // that might resize the sequence or destroy it.
  //
  // Initializing non-trivial elements must be done using an
  // uninitialized assignment (see e.g., std::uninitialized_copy
  // or std::uninitialized_move). Ordinary assingment will trigger
  // the destructor of the uninitialized contents which is bad.
  static sequence<value_type> uninitialized(size_t n) {
    return sequence<value_type>(n, _uninitialized_tag{});
  }

  // Create a sequence of length n consisting of the elements
  // generated by f(0), f(1), ..., f(n-1)
  template<typename F>
  static sequence<value_type> from_function(size_t n, F&& f) {
    auto s = sequence<value_type>::uninitialized(n);
    auto buffer = s.data();
    parallel_for(0, n, [&](size_t i) {
      s.impl.initialize(&buffer[i], f(i));
    });
    return s;
  }

 private:

  struct _uninitialized_tag { };

  sequence(size_t n, _uninitialized_tag) : _sequence_base<T, Allocator>() {
    impl.ensure_capacity(n);
    impl.set_size(n);
#ifdef PARLAY_DEBUG_UNINITIALIZED
    if constexpr (std::is_same_v<value_type, debug_uninitialized>) {
      auto buffer = impl.data();
      parallel_for(0, n, [&](size_t i) {
        buffer[i].x = -1;
      });
    }
#endif
  }

  void initialize_default(size_t n) {
    initialize_fill(n, value_type{});
  }

  void initialize_fill(size_t n, const value_type& v) {
    impl.ensure_capacity(n);
    auto buffer = impl.data();
    parallel_for(0, n, [&](size_t i) {
      impl.initialize_explicit(buffer + i, v);
    });
    impl.set_size(n);
  }

  template<typename _InputIterator>
  void initialize_range(_InputIterator first, _InputIterator last, std::input_iterator_tag) {
    for (; first != last; first++) {
      push_back(*first);
    }
  }

  template<typename _ForwardIterator>
  void initialize_range(_ForwardIterator first, _ForwardIterator last, std::forward_iterator_tag) {
    auto n = std::distance(first, last);
    impl.ensure_capacity(n);
    auto buffer = impl.data();
    for (size_t i = 0; first != last; i++, first++) {
      impl.initialize_explicit(buffer + i, *first);
    }
    impl.set_size(n);
  }
  
  template<typename _RandomAccessIterator>
  void initialize_range(_RandomAccessIterator first, _RandomAccessIterator last, std::random_access_iterator_tag) {
    auto n = std::distance(first, last);
    impl.ensure_capacity(n);
    auto buffer = impl.data();
    parallel_for(0, n, [&](size_t i) {
      impl.initialize_explicit(buffer + i, first[i]);
    });
    impl.set_size(n);
  }
  
  // Use tag dispatch to distinguish between the (n, value)
  // constructor and the iterator pair constructor
  
  template<typename _Integer>
  void initialize_dispatch(_Integer n, _Integer v, std::true_type) {
    initialize_fill(n, v);
  }
  
  template<typename _Iterator>
  void initialize_dispatch(_Iterator first, _Iterator last, std::false_type) {
    initialize_range(first, last, typename
      std::iterator_traits<_Iterator>::iterator_category());
  }
  
  // Use tag dispatch to distinguish between the (n, value)
  // append operation and the iterator pair append operation
  
  template<typename _Integer>
  iterator append_dispatch(_Integer first, _Integer last, std::true_type) {
    return append_n(first, last);
  }
  
  template<typename _Iterator>
  iterator append_dispatch(_Iterator first, _Iterator last, std::false_type) {
    return append_range(first, last, typename
      std::iterator_traits<_Iterator>::iterator_category());
  }
  
  iterator append_n(size_t n, const value_type& t) {
    impl.ensure_capacity(size() + n);
    auto it = end();
    parallel_for(0, n, [&](size_t i) { impl.initialize_explicit(it + i, t); });
    impl.set_size(size() + n);
    return it;
  }
  
  // Distinguish between different types of iterators
  // InputIterators ad FowardIterators can not be used
  // in parallel, so they operate sequentially.
  
  template<typename _InputIterator>
  iterator append_range(_InputIterator first, _InputIterator last, std::input_iterator_tag) {
    size_t n = 0;
    for (; first != last; first++, n++) {
      push_back(*first);
    }
    return end() - n;
  }
  
  template<typename _ForwardIterator>
  iterator append_range(_ForwardIterator first, _ForwardIterator last, std::forward_iterator_tag) {
    auto n = std::distance(first, last);
    impl.ensure_capacity(size() + n);
    auto it = end();
    std::uninitialized_copy(first, last, it);
    impl.set_size(size() + n);
    return it;
  }
  
  template<typename _RandomAccessIterator>
  iterator append_range(_RandomAccessIterator first, _RandomAccessIterator last, std::random_access_iterator_tag) {
    auto n = std::distance(first, last);
    impl.ensure_capacity(size() + n);
    auto it = end();
    parallel_for(0, n, [&](size_t i) { impl.initialize_explicit(it + i, first[i]); });
    impl.set_size(size() + n);
    return it;
  }
  
  // Use tag dispatch to distinguish between the (n, value)
  // insert operation and the iterator pair insert operation
  
  template<typename _Integer>
  iterator insert_dispatch(iterator p, _Integer n, _Integer v, std::true_type) {
    return insert_n(p, n, v);
  }
  
  template<typename _Iterator>
  iterator insert_dispatch(iterator p, _Iterator first, _Iterator last, std::false_type) {
    return insert_range(p, first, last);
  }
  
  iterator insert_n(iterator p, size_t n, const value_type& v) {
    // p might be invalidated when the capacity is increased,
    // so we have to remember where it is in the sequence
    auto pos = p - begin();
    impl.ensure_capacity(size() + n);
    p = begin() + pos;

    // Note that "it" is guaranteed to remain valid even after
    // the call to move_append since we ensured that there was
    // sufficient capacity already, so a second reallocation will
    // never happen after this point
    auto the_tail = pop_tail(p);
    auto it = append_n(n, v);
    move_append(the_tail);
    return it;
  }
  
  template<typename _Iterator>
  iterator insert_range(iterator p, _Iterator first, _Iterator last) {
    auto the_tail = pop_tail(p);
    auto it = append(first, last);
    auto pos = it - begin();
    move_append(the_tail);
    return begin() + pos;
  }
  
  // Append the given range, moving its elements into this sequence
  template<typename R>
  void move_append(R&& r) {
    append(std::make_move_iterator(std::begin(r)),
      std::make_move_iterator(std::end(r)));
  }
  
  // Return true if this sequence compares equal to the sequence
  // beginning at other. The sequence beginning at other must be
  // of at least the same length as this sequence.
  template<typename _Iterator>
  bool compare_equal(_Iterator other, size_t granularity = 1000) const {
    auto n = size();
    auto self = begin();
    size_t i;
    for (i = 0; i < (std::min)(granularity, n); i++)
      if (!(self[i] == other[i])) return false;
    if (i == n) return true;
    size_t start = granularity;
    size_t block_size = 2 * granularity;
    bool matches = true;
    while (start < n) {
      size_t last = (std::min)(n, start + block_size);
      parallel_for(start, last, [&](size_t j) {
        if (!(self[j] == other[j])) matches = false;
      }, granularity);
      if (!matches) return false;
      start += block_size;
      block_size *= 2;
    }
    return matches;
  }
  
};

// Convert an arbitrary range into a sequence
template<typename R>
inline auto to_sequence(R&& r) -> sequence<range_value_type_t<R>> {
  if constexpr (std::is_lvalue_reference<R>::value) {
    return {std::begin(r), std::end(r)};
  }
  else {
    return {std::make_move_iterator(std::begin(r)),
            std::make_move_iterator(std::end(r))};
  }
}

}  // namespace parlay

namespace std {

// exchange the values of a and b
template<typename T, typename Allocator>
inline void swap(parlay::sequence<T, Allocator>& a, parlay::sequence<T, Allocator>& b) {
  a.swap(b);
}

}  // namespace std

#endif  // PARLAY_SEQUENCE_H_
