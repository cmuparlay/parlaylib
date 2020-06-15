// A sequence is a dynamic array supporting parallel
// modification operations. It can be thought of as a
// parallel version of std::vector.

#include <initializer_list>
#include <iterator>
#include <iostream>
#include <memory>
#include <type_traits>

#include "parallel.h"
#include "range.h"

namespace parlay {

// This base class handles memory allocation for sequences.
// We inherit from Allocator to employ empty base optimization
// so that the size of the sequence type is not increased when
// the allocator is stateless.
//
// This base class handles whether the sequence is big or small
// (small size optimized). Conversion from small to large and
// all allocations are handled here. The main sequence class
// handles the higher level logic that is agnostic to the memory
// under the hood.
template <typename T, typename Allocator>
struct _sequence_base {

  using allocator_type = Allocator;
  using byte_type = unsigned char;
  using value_type = T;

  // This class handles internal memory allocation and whether
  // the sequence is big or small. Currently only works for
  // little endian systems. Specifically, the union type stores
  // one of two bitfields,
  //
  // LONG:
  //  void*          buffer     ---->   size_t        capacity
  //  uint64_t : 63  n                  value_type[]  elements
  //  uint64_t : 1   flag
  //
  // SHORT:
  //  unsigned char[15]  buffer
  //  unsigned char : 7  n
  //  unsigned char : 1  flag
  //
  // And we just pray that the flag bits line up and go in the
  // same bit for both of them. This should work for little endian
  // systems in general, but it's technically undefined behaviour
  // because it requires type punning the union.
  //
  struct _sequence_impl : public allocator_type {

    // Empty constructor
    _sequence_impl() : allocator_type(), _data() { }

    // Copy constructor
    _sequence_impl(const _sequence_impl& other) {
      auto n = other.size();
      ensure_capacity(n);
      auto buffer = data();
      auto other_buffer = other.data();
      parallel_for(0, n, [&](size_t i) {
        initialize(buffer + i, other_buffer[i]);
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
      move_from(other);
    }

    // Swap a sequence backend with another
    void swap(_sequence_impl& other) {
      auto tmp = _sequence_impl(std::move(other));
      other = std::move(*this);
      *this = std::move(tmp);
    }
    
    // Destroy a sequence backend
    ~_sequence_impl() {
      clear();
    }

    // Move the contents of other into this sequence
    // Assumes that this sequence is empty.
    void move_from(_sequence_impl&& other) {
      if (other.is_small()) {
        // Move initialize the sequence from the elements of the old one
        auto buffer = data();
        auto other_buffer = other._data.small.data();
        parallel_for(0, other._data.small.n, [&](size_t i) {
          initialize(buffer + i, std::move(other_buffer[i]));
          other_buffer[i].~value_type();
        });
        _data.small.n = other._data.small.n;
        _data.small.flag = 0;
      }
      else {
        // Steal the buffer from the other sequence.
        // Moving individual elements is not required
        _data.large.buffer = other._data.large.buffer;
        _data.large.n = other._data.large.n;
        _data.large.flag = 1;        
      }
      other._data.small.n = 0;
      other._data.small.flag = 0;
    }

    // Call the destructor on all elements
    void destroy_all() {
      if (!std::is_trivially_destructible<value_type>::value) {
        auto n = size();
        auto current_buffer = data();
        parallel_for(0, n, [&](size_t i) {
          current_buffer[i].~value_type();
        });
      }
    }

    // Destroy all elements, free the buffer (if any)
    // and set the sequence to the empty sequence
    void clear() {
      destroy_all();
      if (!is_small()) {
        _data.large.buffer.free_buffer(*this);
      }
      _data.small.n = 0;
      _data.small.flag = 0;
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

      ~capacitated_buffer() {
        assert(buffer == nullptr);
      }

      void free_buffer(allocator_type& a) {
        a.deallocate((value_type*)buffer, get_capacity() + offset);
        buffer = nullptr;
      }

      void set_capacity(size_t capacity) {
        *((size_t*)buffer) = capacity;
      }

      size_t get_capacity() const {
        return *((size_t*)buffer);
      }

      value_type* data() {
        return (value_type*)buffer + offset;
      }

      const value_type* data() const {
        return (value_type*)buffer + offset;
      }

      void* buffer;
    };

    // A not-short-size-optimized sequence. Elements are
    // stored in a heap-allocated buffer.
    struct _long {
      capacitated_buffer buffer;
      uint64_t n : 63;
      uint64_t flag : 1;
      
      size_t capacity() const {
        return buffer.get_capacity();
      }
      
      value_type* data() {
        return buffer.data();
      }
      
      const value_type* data() const {
        return buffer.data();
      }
    };

    // A short-size-optimized sequence. Elements are stored
    // inline in the data structure.
    struct _short {
      byte_type buffer[15];
      unsigned char n : 7;
      unsigned char flag : 1;
      
      _short() : n(0), flag(0) { }
      
      size_t capacity() const {
        return (sizeof(_long) - 1) / sizeof(value_type);
      }
      
      value_type* data() {
        return (value_type*)&buffer;
      }
      
      const value_type* data() const {
        return (value_type*)&buffer;
      }
    };

    // Store either a short or a long sequence
    // By default, we store an empty short sequence
    union _data_impl {
      _data_impl() : small() { }
      _long large;
      _short small;
    } _data;

    // Returns true if the sequence is in small size mode
    bool is_small() const {
      return sizeof(value_type) <= sizeof(size_t) &&
        _data.small.flag == 0;
    }

    // Return the size of the sequence
    size_t size() const {
      if (is_small()) return _data.small.n;
      else return _data.large.n;
    }

    // Return the capacity of the sequence
    size_t capacity() const {
      if (is_small()) return _data.small.capacity();
      else return _data.large.capacity();
    }

    value_type* data() {
      if (is_small()) return _data.small.data();
      else return _data.large.data();
    }
    
    const value_type* data() const {
      if (is_small()) return _data.small.data();
      else return _data.large.data();
    }

    void set_size(size_t new_size) {
      assert(new_size <= capacity());
      if (is_small()) _data.small.n = new_size;
      else _data.large.n = new_size;
    }
    
    // Constructs am object of type value_type at an
    // uninitialized memory location p using args...
    template<typename... Args>
    void initialize(value_type* p, Args&&... args) {
      std::allocator_traits<allocator_type>::construct(
        *this, p, std::forward<Args>(args)...);
    }
    
    // Destroy the object of type value_type pointed to by p
    void destroy(value_type* p) {
      std::allocator_traits<allocator_type>::destroy(p);
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
        size_t new_capacity = std::max(desired,
          (15 * current + 9) / 10);
        capacitated_buffer new_buffer(new_capacity, *this);
        
        // Move initialize the new buffer with the
        // contents of the old buffer
        auto n = size();
        auto dest_buffer = new_buffer.data();
        auto current_buffer = data();
        parallel_for(0, n, [&](size_t i) {
          initialize(dest_buffer + i, std::move(current_buffer[i]));
          current_buffer[i].~value_type();
        });
        
        // Destroy the old stuff
        if (!is_small()) {
          _data.large.buffer.free_buffer(*this);
        }
        
        // Assign the new stuff
        _data.large.buffer = new_buffer;
        _data.large.n = n;
        _data.large.flag = 1;
      }
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
template <typename T, typename Allocator = std::allocator<T>>
class sequence : protected _sequence_base<T, Allocator> {
 public:

  // --------------- Container requirements ---------------

  using value_type = T;
  using reference = T&;
  using const_reference = const T&;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  
  using _sequence_base<T, Allocator>::impl;

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
    std::swap(*this, b);
    return *this;
  }

  // destroys all elements and frees all memory
  ~sequence() = default;

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
    return size() == other.size() &&
      std::equal(begin(), end(), other.begin(), other.end());  // TODO: In parallel
  }

  bool operator!=(const sequence<T, Allocator>& b) const {
    return !(*this == b);
  }

  void swap(sequence<T, Allocator>& b) {
    impl.swap(b.impl); 
  }

  size_type size() const { return impl.size(); }

  size_type max_size() const { return (1LL << (sizeof(size_type)-1)) - 1; }

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

  // Constructs a sequence from the elements of the
  // given initializer list
  sequence(std::initializer_list<value_type> l) : _sequence_base<T, Allocator>() {
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
      impl.ensure_capacity(size() + 1);  // Required to make sure that the
      auto the_tail = pop_tail(p);       // emplace_back iterator remains valid
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
  
  template<PARLAY_RANGE_TYPE R>
  iterator append(R&& r) {
    if (std::is_lvalue_reference<R>::value) {
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

  template<PARLAY_RANGE_TYPE R>
  iterator insert(iterator p, R&& r) {
    if (std::is_lvalue_reference<R>::value) {
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
  
  void resize(size_t new_size, const value_type& v = value_type{}) {
    auto current = size();
    auto buffer = impl.data();
    if (new_size < current) {
      parallel_for(new_size, current, [&](size_t i) {
        impl.destroy(&buffer[i], v);
      });
    }
    else {
      impl.ensure_capacity(new_size);
      auto buffer = impl.data();
      parallel_for(current, new_size, [&](size_t i) {
        impl.initialize(&buffer[i], v);
      });
    }
    impl.set_size(new_size);
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
    impl.clear();
    initialize_range(std::begin(l), std::end(l),
      std::iterator_traits<decltype(std::begin(l))>::iterator_category());
  }

  value_type& front() { return *begin(); }
  value_type& back() { return *(end() - 1); }
  
  const value_type& front() const { return *begin(); }
  const value_type& back() const { return *(end() - 1); }
  
  sequence<value_type> head(iterator p) {
    return sequence<value_type>{begin(), p};
  }
  
  sequence<value_type> head(size_t len) {
    return sequence<value_type>{begin(), begin() + len};
  }

  sequence<value_type> tail(iterator p) {
    return sequence<value_type>{p, end()};
  }
  
  sequence<value_type> tail(size_t len) {
    return tail(end() - len);
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

 private:
  
  void initialize_default(size_t n) {
    initialize_fill(n, value_type{});
  }
  
  void initialize_fill(size_t n, const value_type& v) {
    impl.ensure_capacity(n);
    auto buffer = impl.data();
    parallel_for(0, n, [&](size_t i) {
      impl.initialize(buffer + i, v);
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
    std::uninitialized_copy(first, last, impl.data());
    impl.set_size(n);
  }
  
  template<typename _RandomAccessIterator>
  void initialize_range(_RandomAccessIterator first, _RandomAccessIterator last, std::random_access_iterator_tag) {
    auto n = std::distance(first, last);
    impl.ensure_capacity(n);
    auto buffer = impl.data();
    parallel_for(0, n, [&](size_t i) {
      impl.initialize(buffer + i, first[i]);
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
    initialize_range(first, last, std::iterator_traits<_Iterator>::iterator_category());
  }
  
  // Use tag dispatch to distinguish between the (n, value)
  // append operation and the iterator pair append operation
  
  template<typename _Integer>
  iterator append_dispatch(_Integer first, _Integer last, std::true_type) {
    append_n(first, last);
  }
  
  template<typename _Iterator>
  iterator append_dispatch(_Iterator first, _Iterator last, std::false_type) {
    append_range(first, last, std::iterator_traits<_Iterator>::iterator_category());
  }
  
  iterator append_n(size_t n, const value_type& t) {
    impl.ensure_capacity(size() + n);
    auto it = end();
    parallel_for(0, n, [&](size_t i) { impl.initialize(it + i, t); });
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
    parallel_for(0, n, [&](size_t i) { impl.initialize(it + i, first[i]); });
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
    impl.ensure_capacity(size() + n);
    auto the_tail = pop_tail(p);
    auto it = append_n(n, v);
    move_append(the_tail);
    impl.set_size(size() + n);
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
  template<PARLAY_RANGE_TYPE R>
  void move_append(R&& r) {
    append(std::make_move_iterator(std::begin(r)), std::make_move_iterator(std::end(r)));
  }
  
};

// exchange the values of a and b
template<typename T, typename Allocator>
inline void swap(sequence<T, Allocator>& a, sequence<T, Allocator>& b) {
  a.swap(b);
}

// Convert an arbitrary range into a sequence
template<PARLAY_RANGE_TYPE R>
auto to_sequence(R&& r) -> sequence<typename std::remove_const<
                            typename std::remove_reference<
                            decltype(*std::begin(std::declval<R&>()))
                            >::type>::type> {
  if (std::is_lvalue_reference<R>::value) {
    return {std::begin(r), std::end(r)};
  }
  else {
    return {std::make_move_iterator(std::begin(r)),
            std::make_move_iterator(std::end(r))};
  }
}

}  // namespace parlay
