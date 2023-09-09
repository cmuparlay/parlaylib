// A sequence is a dynamic array supporting parallel modification operations. It can be thought of as a
// parallel version of std::vector.
//
// Parlay sequences also support optional small-size optimization, where short sequences of trivial
// types are stored inline in the object rather than allocated on the heap. By default, small-size
// optimization is not enabled. A type alias, short_sequence, is provided, that turns on small-size
// optimization.
//

#ifndef PARLAY_SEQUENCE_H_
#define PARLAY_SEQUENCE_H_

#include <cassert>
#include <cstddef>

#include <functional>          // IWYU pragma: keep
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Falsely suggested for std::hash
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_include <variant>

#include "alloc.h"
#include "parallel.h"
#include "portability.h"
#include "range.h"
#include "slice.h"
#include "type_traits.h"      // IWYU pragma: keep  // for is_trivially_relocatable
#include "utilities.h"

#include "internal/sequence_base.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "internal/debug_uninitialized.h"
#endif

namespace parlay {

// If the macro PARLAY_USE_STD_ALLOC is defined, sequences will default
// to using std::allocator instead of parlay::allocator.
namespace internal {
#ifndef PARLAY_USE_STD_ALLOC
template<typename T>
using sequence_default_allocator = parlay::allocator<T>;
#else
template<typename T>
using sequence_default_allocator = std::allocator<T>;
#endif
}  // namespace internal


// A sequence is a dynamic array supporting parallel modification operations.
// It is designed to be a fully-parallel drop-in replacement for std::vector.
//
// Template arguments:
//  T:          the value type of the sequence
//  Allocator:  an allocator for type T
//  EnableSSO:  true to enable small-size optimization
//
template<typename T, typename Allocator = internal::sequence_default_allocator<T>, bool EnableSSO = std::is_same<T, char>::value>
class sequence : protected sequence_internal::sequence_base<T, Allocator, EnableSSO> {

  static_assert(std::is_same_v<typename std::remove_cv_t<T>, T>, "sequences must have a non-const, non-volatile value_type");
  static_assert(std::is_same_v<typename std::decay_t<T>, T>, "sequences must not have an array, reference, or function value_type");
  static_assert(!std::is_void_v<T>, "sequences must not have a void value_type");
  static_assert(std::is_destructible_v<T>, "sequences must have a destructible value_type");

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

  using sequence_type = sequence<T, Allocator, EnableSSO>;
  using sequence_base_type = sequence_internal::sequence_base<T, Allocator, EnableSSO>;
  using allocator_type = Allocator;

  using sequence_base_type::storage;
  using sequence_base_type::_max_size;
  using sequence_base_type::copy_granularity;
  using sequence_base_type::initialization_granularity;

  // creates an empty sequence
  sequence() : sequence_base_type() {}

  // creates a copy of s
  sequence(const sequence_type& s) : sequence_base_type(s.storage) {}

  // moves rv
  sequence(sequence_type&& rv) noexcept : sequence_base_type(std::move(rv.storage)) {}

  // copy and move assignment
  sequence_type& operator=(sequence_type b) {
    swap(b);
    return *this;
  }

#ifdef PARLAY_DEBUG_UNINITIALIZED
  // If uninitialized memory debugging is turned on, make sure that
  // each object of type UninitializedTracker has been initialized
  // since we are about to run their destructors!
  ~sequence() {
    auto buffer = storage.data();
    parallel_for(0, size(), [&](size_t i) { PARLAY_ASSERT_INITIALIZED(buffer[i]); });
  }
#endif

  allocator_type get_allocator() const { return storage.get_allocator(); }

  iterator begin() { return storage.data(); }
  iterator end() { return storage.data() + storage.size(); }

  const_iterator begin() const { return storage.data(); }
  const_iterator end() const { return storage.data() + storage.size(); }

  const_iterator cbegin() const { return storage.data(); }
  const_iterator cend() const { return storage.data() + storage.size(); }

  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const { return std::make_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return std::make_reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }

  bool operator==(const sequence_type& other) const { return size() == other.size() && compare_equal(other.begin()); }

  bool operator!=(const sequence_type& b) const { return !(*this == b); }

  void swap(sequence_type& b) { storage.swap(b.storage); }

  size_type size() const { return storage.size(); }

  size_type max_size() const {
    if ((std::numeric_limits<size_type>::max)() < _max_size) {
      return (std::numeric_limits<size_type>::max)();
    } else {
      return _max_size;
    }
  }

  bool empty() const { return size() == 0; }

  // -------------- Random access interface ----------------

  value_type* data() { return storage.data(); }

  const value_type* data() const { return storage.data(); }

  value_type& operator[](size_t i) { return storage.at(i); }
  const value_type& operator[](size_t i) const { return storage.at(i); }

  value_type& at(size_t i) {
    if (i >= size()) {
      throw_exception_or_terminate<std::out_of_range>(
          "sequence access out of bounds: length = " + std::to_string(size()) + ", index = " + std::to_string(i));
    } else {
      return storage.at(i);
    }
  }

  const value_type& at(size_t i) const {
    if (i >= size()) {
      throw_exception_or_terminate<std::out_of_range>(
          "sequence access out of bounds: length = " + std::to_string(size()) + ", index = " + std::to_string(i));
    } else {
      return storage.at(i);
    }
  }

  // ------------ SequenceContainer requirements ---------------

  // Construct a sequence of length n. Elements will be
  // value initialized
  explicit sequence(size_t n) : sequence_base_type() { initialize_default(n); }

  // Constructs a sequence consisting of n copies of t
  sequence(size_t n, const value_type& t) : sequence_base_type() { initialize_fill(n, t); }

  // Constructs a sequence consisting of the elements
  // in the given iterator range
  template<typename Iterator_>
  sequence(Iterator_ i, Iterator_ j) : sequence_base_type() {
    initialize_dispatch(i, j, std::is_integral<Iterator_>());
  }

  // Constructs a sequence from the elements of the given initializer list
  //
  // Note: cppcheck flags all implicit constructors. This one is okay since
  // we want to convert initializer lists into sequences.
  sequence(std::initializer_list<value_type> l) :
      sequence_base_type() {  // cppcheck-suppress noExplicitConstructor
    initialize_range(std::begin(l), std::end(l),
                     typename std::iterator_traits<decltype(std::begin(l))>::iterator_category());
  }

  sequence_type& operator=(std::initializer_list<value_type> l) {
    storage.clear();
    initialize_range(std::begin(l), std::end(l),
                     typename std::iterator_traits<decltype(std::begin(l))>::iterator_category());
    return *this;
  }

  template<typename... Args>
  iterator emplace(iterator p, Args&&... args) {
    if (p == end()) {
      return emplace_back(std::forward<Args>(args)...);
    } else {
      // p might be invalidated when the capacity is increased,
      // so we need to remember where it occurs in the sequence
      auto pos = p - begin();
      storage.ensure_capacity(size() + 1);
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
    storage.ensure_capacity(size() + 1);
    storage.initialize(end(), std::forward<Args>(args)...);
    storage.set_size(size() + 1);
    return end() - 1;
  }

  iterator push_back(const value_type& v) { return emplace_back(v); }

  iterator push_back(value_type&& v) { return emplace_back(std::move(v)); }

  template<typename Iterator_>
  iterator append(Iterator_ first, Iterator_ last) {
    return append_dispatch(first, last, std::is_integral<Iterator_>());
  }

  template<typename R>
  iterator append(R&& r) {
    return append(std::begin(r), std::end(r));
  }

  // Append the given sequence r. Since r is an rvalue, we can
  // move its elements instead of copying them. Furthermore, if
  // the current sequence is empty and doesn't own a large buffer,
  // we can simply move assign the entire sequence r
  iterator append(sequence_type&& r) {
    if (empty() && capacity() <= r.size()) {
      *this = std::move(r);
      return begin();
    } else {
      return append(std::make_move_iterator(std::begin(r)), std::make_move_iterator(std::end(r)));
    }
  }

  iterator append(size_t n, const value_type& t) { return append_n(n, t); }

  iterator insert(iterator p, const value_type& t) { return emplace(p, t); }

  iterator insert(iterator p, value_type&& rv) { return emplace(p, std::move(rv)); }

  iterator insert(iterator p, size_t n, const value_type& t) { return insert_n(p, n, t); }

  template<typename Iterator_>
  iterator insert(iterator p, Iterator_ i, Iterator_ j) {
    return insert_dispatch(p, i, j, std::is_integral<Iterator_>());
  }

  template<typename Range,
    std::enable_if_t<!std::is_same_v<std::decay_t<Range>, value_type> &&
                      is_input_range_v<Range> &&
                      std::is_constructible_v<value_type, range_reference_type_t<Range>>, int> = 0>
  iterator insert(iterator p, Range&& r) {
    return insert(p, std::begin(r), std::end(r));
  }

  iterator insert(iterator p, sequence_type&& r) {
    return insert(p, std::make_move_iterator(std::begin(r)), std::make_move_iterator(std::end(r)));
  }

  iterator insert(iterator p, std::initializer_list<value_type> l) {
    return insert_range(p, std::begin(l), std::end(l));
  }

  iterator erase(iterator q) {
    if (q == end() - 1) {
      pop_back();
      return end();
    } else {
      auto pos = q - begin();
      auto the_tail = pop_tail(q + 1);
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
    storage.destroy(end() - 1);
    storage.set_size(size() - 1);
  }

  // if all elements have been relocated out of this sequence then don't
  // destroy them (it would not only be inefficient, but incorrect).
  // Note that this is all or none: i.e. they better all be relocated for
  // this function, or none for the standard destructor or clear().
  // It is not a member function, so as to discourage its use by naive users.
  friend void clear_relocated(sequence& S) { S.storage.clear_without_destruction(); }

  void clear() { storage.clear(); }


  void resize(size_t new_size, const value_type& v = value_type()) {
    auto current = size();
    if (new_size < current) {
      if constexpr (!std::is_trivially_destructible_v<value_type>) {
        auto buffer = storage.data();
        parallel_for(new_size, current, [&](size_t i) { storage.destroy(&buffer[i]); });
      }
    } else {
      storage.ensure_capacity(new_size);
      assert(storage.capacity() >= new_size);
      auto buffer = storage.data();
      parallel_for(
          current, new_size, [&](size_t i) { storage.initialize_explicit(&buffer[i], v); },
          copy_granularity(new_size - current));
    }
    storage.set_size(new_size);
  }

  void reserve(size_t amount) {
    storage.ensure_capacity(amount);
    assert(storage.capacity() >= amount);
  }

  size_t capacity() const { return storage.capacity(); }

  template<typename Iterator_>
  void assign(Iterator_ i, Iterator_ j) {
    storage.clear();
    initialize_dispatch(i, j, std::is_integral<Iterator_>());
  }

  void assign(size_t n, const value_type& v) {
    storage.clear();
    return initialize_fill(n, v);
  }

  void assign(std::initializer_list<value_type> l) { assign(std::begin(l), std::end(l)); }

  template<typename R>
  void assign(R&& r) {
    assign(std::begin(r), std::end(r));
  }

  void assign(sequence_type&& r) { *this = std::move(r); }

  value_type& front() { return *begin(); }
  value_type& back() { return *(end() - 1); }

  const value_type& front() const { return *begin(); }
  const value_type& back() const { return *(end() - 1); }

  auto head(iterator p) { return make_slice(begin(), p); }

  auto head(size_type len) { return make_slice(begin(), begin() + len); }

  auto tail(iterator p) { return make_slice(p, end()); }

  auto tail(size_type len) { return make_slice(end() - len, end()); }

  auto cut(size_type s, size_type e) { return make_slice(begin() + s, begin() + e); }

  // Const versions of slices

  auto head(iterator p) const { return make_slice(begin(), p); }

  auto head(size_type len) const { return make_slice(begin(), begin() + len); }

  auto tail(iterator p) const { return make_slice(p, end()); }

  auto tail(size_type len) const { return make_slice(end() - len, end()); }

  auto cut(size_type s, size_type e) const { return make_slice(begin() + s, begin() + e); }

  auto substr(size_type pos) const { return to_sequence(cut(pos, size())); }

  auto substr(size_type pos, size_type count) const { return to_sequence(cut(pos, pos + count)); }

  auto subseq(size_type s, size_type e) const { return to_sequence(cut(s,e)); }

  // Remove all elements of the subsequence beginning at the element
  // pointed to by p, and return a new sequence consisting of those
  // removed elements.
  sequence_type pop_tail(iterator p) {
    if (p == end()) {
      return sequence_type{};
    } else {
      sequence_type the_tail(std::make_move_iterator(p), std::make_move_iterator(end()));
      if constexpr (!std::is_trivially_destructible_v<value_type>) {
        parallel_for(0, end() - p, [&](size_t i) { storage.destroy(&p[i]); });
      }
      storage.set_size(p - begin());
      return the_tail;
    }
  }

  // Remove the last len elements from the sequence and return
  // a new sequence consisting of the removed elements
  sequence_type pop_tail(size_t len) { return pop_tail(end() - len); }

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
  // or std::uninitialized_move). Ordinary assignment will trigger
  // the destructor of the uninitialized contents which is bad.
  static sequence_type uninitialized(size_t n) { return sequence_type(n, _uninitialized_tag{}); }

#ifdef _MSC_VER
  #pragma warning(push)
#pragma warning(disable : 4267)  // conversion from 'size_t' to *, possible loss of data
#endif

  // Create a sequence of length n consisting of the elements
  // generated by f(0), f(1), ..., f(n-1)
  template<typename F>
  static sequence_type from_function(size_t n, F&& f, size_t granularity = 0) {
    return sequence_type(n, std::forward<F>(f), _from_function_tag(), granularity);
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  std::vector<T> to_vector() const {
    return {begin(), end()};
  }

 private:
  struct _uninitialized_tag {};
  struct _from_function_tag {};

  sequence(size_t n, _uninitialized_tag) : sequence_base_type() {
    storage.initialize_capacity(n);
    storage.set_size(n);

    // If uninitialized memory debugging is turned on, make sure that
    // a buffer of UninitializedTracker is appropriately set to its
    // uninitialized state.
#ifdef PARLAY_DEBUG_UNINITIALIZED
    if constexpr (std::is_same_v<value_type, internal::UninitializedTracker>) {
      auto buffer = storage.data();
      parallel_for(0, n, [&](size_t i) {
        buffer[i].initialized = false;
        PARLAY_ASSERT_UNINITIALIZED(buffer[i]);
      });
    }
#endif
  }

  template<typename F>
  sequence(size_t n, F&& f, _from_function_tag, size_t granularity = 0) :
      sequence_base_type() {
    // value_type must either be constructible from f(i), or f(i) must return a prvalue
    // of value_type, in which case we can rely on guaranteed copy elision
    static_assert(std::is_constructible_v<value_type, std::invoke_result_t<F&&, size_t>> ||
                  std::is_same_v<value_type, std::invoke_result_t<F&&, size_t>>);

    storage.initialize_capacity(n);
    storage.set_size(n);
    auto buffer = storage.data();
    parallel_for(0, n, [&](size_t i) {
      if constexpr (std::is_constructible_v<value_type, std::invoke_result_t<F&&, size_t>>) {
        storage.initialize(&buffer[i], f(i));
      }
      else {
        storage.initialize_with_copy_elision(&buffer[i], [&]() { return f(i); });
      }
    }, granularity);
  }

  // Implement initialize_default manually rather than
  // calling initialize_fill(n, value_type()) because
  // this allows us to store a sequence of uncopyable
  // types provided that no reallocation ever happens.
  void initialize_default(size_t n) {
    storage.initialize_capacity(n);
    auto buffer = storage.data();
    parallel_for(0, n, [&](size_t i) {      // Calling initialize with
      storage.initialize(buffer + i);       // no arguments performs
    }, initialization_granularity(n));      // value initialization
    storage.set_size(n);
  }

  void initialize_fill(size_t n, const value_type& v) {
    storage.initialize_capacity(n);
    auto buffer = storage.data();
    parallel_for(0, n, [&](size_t i) {
      storage.initialize_explicit(buffer + i, v);
    }, copy_granularity(n));
    storage.set_size(n);
  }

  template<typename InputIterator_>
  void initialize_range(InputIterator_ first, InputIterator_ last, std::input_iterator_tag) {
    for (; first != last; ++first) {
      push_back(*first);
    }
  }

  template<typename ForwardIterator_>
  void initialize_range(ForwardIterator_ first, ForwardIterator_ last, std::forward_iterator_tag) {
    auto n = std::distance(first, last);
    storage.initialize_capacity(n);
    auto buffer = storage.data();
    for (size_t i = 0; first != last; i++, ++first) {
      storage.initialize(buffer + i, *first);
    }
    storage.set_size(n);
  }

  template<typename RandomAccessIterator_>
  void initialize_range(RandomAccessIterator_ first, RandomAccessIterator_ last, std::random_access_iterator_tag) {
    auto n = std::distance(first, last);
    storage.initialize_capacity(n);
    auto buffer = storage.data();
    parallel_for(0, n, [&](size_t i) {
      storage.initialize(buffer + i, first[i]);
    }, copy_granularity(n));
    storage.set_size(n);
  }

  // Use tag dispatch to distinguish between the (n, value)
  // constructor and the iterator pair constructor

  template<typename Integer_>
  void initialize_dispatch(Integer_ n, Integer_ v, std::true_type) {
    initialize_fill(n, v);
  }

  template<typename Iterator_>
  void initialize_dispatch(Iterator_ first, Iterator_ last, std::false_type) {
    initialize_range(first, last, typename std::iterator_traits<Iterator_>::iterator_category());
  }

  // Use tag dispatch to distinguish between the (n, value)
  // append operation and the iterator pair append operation

  template<typename Integer_>
  iterator append_dispatch(Integer_ first, Integer_ last, std::true_type) {
    return append_n(first, last);
  }

  template<typename Iterator_>
  iterator append_dispatch(Iterator_ first, Iterator_ last, std::false_type) {
    return append_range(first, last, typename std::iterator_traits<Iterator_>::iterator_category());
  }

  iterator append_n(size_t n, const value_type& t) {
    storage.ensure_capacity(size() + n);
    auto it = end();
    parallel_for(0, n, [&](size_t i) {
      storage.initialize_explicit(it + i, t);
    }, this->copy_granularity(n));
    storage.set_size(size() + n);
    return it;
  }

  // Distinguish between different types of iterators
  // InputIterators and ForwardIterators can not be used
  // in parallel, so they operate sequentially.

  template<typename InputIterator_>
  iterator append_range(InputIterator_ first, InputIterator_ last, std::input_iterator_tag) {
    size_t n = 0;
    for (; first != last; first++, n++) {
      push_back(*first);
    }
    return end() - n;
  }

  template<typename ForwardIterator_>
  iterator append_range(ForwardIterator_ first, ForwardIterator_ last, std::forward_iterator_tag) {
    auto n = std::distance(first, last);
    storage.ensure_capacity(size() + n);
    auto it = end();
    std::uninitialized_copy(first, last, it);
    storage.set_size(size() + n);
    return it;
  }

  template<typename RandomAccessIterator_>
  iterator append_range(RandomAccessIterator_ first, RandomAccessIterator_ last, std::random_access_iterator_tag) {
    auto n = std::distance(first, last);
    storage.ensure_capacity(size() + n);
    auto it = end();
    parallel_for(0, n, [&](size_t i) {
      storage.initialize(it + i, first[i]);
    }, copy_granularity(n));
    storage.set_size(size() + n);
    return it;
  }

  // Use tag dispatch to distinguish between the (n, value)
  // insert operation and the iterator pair insert operation

  template<typename Integer_>
  iterator insert_dispatch(iterator p, Integer_ n, Integer_ v, std::true_type) {
    return insert_n(p, n, v);
  }

  template<typename Iterator_>
  iterator insert_dispatch(iterator p, Iterator_ first, Iterator_ last, std::false_type) {
    return insert_range(p, first, last);
  }

  iterator insert_n(iterator p, size_t n, const value_type& v) {
    // p might be invalidated when the capacity is increased,
    // so we have to remember where it is in the sequence
    auto pos = p - begin();
    storage.ensure_capacity(size() + n);
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

  template<typename Iterator_>
  iterator insert_range(iterator p, Iterator_ first, Iterator_ last) {
    auto the_tail = pop_tail(p);
    auto it = append(first, last);
    auto pos = it - begin();
    move_append(the_tail);
    return begin() + pos;
  }

  // Append the given range, moving its elements into this sequence
  template<typename R>
  void move_append(R&& r) {
    append(std::make_move_iterator(std::begin(r)), std::make_move_iterator(std::end(r)));
  }

  // Return true if this sequence compares equal to the sequence
  // beginning at other. The sequence beginning at other must be
  // of at least the same length as this sequence.
  template<typename Iterator_>
  bool compare_equal(Iterator_ other, size_t granularity = 0) const {
    if (granularity == 0) {
      granularity = 1024 * sizeof(size_t) / sizeof(value_type);
    }
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

// A short_sequence is a dynamic array supporting parallel modification operations
// that may also perform small-size optimization. For sequences of trivial types
// whose elements fit in 15 bytes or fewer, the sequence will be stored inline and
// no heap allocation will be performed.
//
// This type is just an alias for parlay::sequence<T, Allocator, true>
//
// Template arguments:
//  T:         the value type of the sequence
//  Allocator: an allocator for type T
//
template<typename T, typename Allocator = internal::sequence_default_allocator<T>>
using short_sequence = sequence<T, Allocator, true>;

// A chars is an alias for a short-size optimized character sequence.
//
// You can think of chars as either an abbreviation of "char sequence",
// or as a plural of char. Both make sense!
//
// Character sequences that fit in 15 bytes or fewer will be stored inline
// without performing a heap allocation. Large sequences are stored on the
// heap, and support update and query operations in parallel.
using chars = sequence<char, internal::sequence_default_allocator<char>, true>;

// Convert an arbitrary range into a sequence.
//
// The value type is deduced from the value type of the range, and the
// default allocator is used.
template<typename R>
inline auto to_sequence(R&& r) -> sequence<range_value_type_t<R>> {
  static_assert(is_random_access_range_v<R> || !is_block_iterable_range_v<R>,
      "You called parlay::to_sequence on a delayed (block-iterable) range. You probably meant to call parlay::delayed::to_sequence");
  return {std::begin(r), std::end(r)};
}

// Convert an arbitrary range into a (short) sequence
//
// The value type is deduced from the value type of the range, and the
// default allocator is used.
template<typename R>
inline auto to_short_sequence(R&& r) -> short_sequence<range_value_type_t<R>> {
  static_assert(is_random_access_range_v<R> || !is_block_iterable_range_v<R>,
      "You called parlay::to_sequence on a delayed (block-iterable) range. You probably meant to call parlay::delayed::to_sequence");
  return {std::begin(r), std::end(r)};
}

// Convert an arbitrary range into a sequence of type T, and optionally
// specify the allocator to use for the sequence.
template<typename T, typename Alloc = internal::sequence_default_allocator<T>, typename R>
inline auto to_sequence(R&& r) -> sequence<T, Alloc> {
  static_assert(is_random_access_range_v<R> || !is_block_iterable_range_v<R>,
      "You called parlay::to_sequence on a delayed (block-iterable) range. You probably meant to call parlay::delayed::to_sequence");
  return {std::begin(r), std::end(r)};
}

// Convert an arbitrary range into a (short) sequence of type T, and optionally
// specify the allocator to use for the sequence.
template<typename T, typename Alloc = internal::sequence_default_allocator<T>, typename R>
inline auto to_short_sequence(R&& r) -> short_sequence<T, Alloc> {
  static_assert(is_random_access_range_v<R> || !is_block_iterable_range_v<R>,
      "You called parlay::to_sequence on a delayed (block-iterable) range. You probably meant to call parlay::delayed::to_sequence");
  return {std::begin(r), std::end(r)};
}

// Mark sequences as trivially relocatable. A sequence is always
// trivially relocatable as long as the allocator is, because:
//  1) Sequences only use small-size optimization when the element
//     type is trivial, so the buffer of trivial elements is
//     trivially relocatable.
//  2) Sequences that are not small-size optimized are just a
//     pointer/length pair, which are trivially relocatable
template<typename T, typename Alloc, bool EnableSSO>
struct is_trivially_relocatable<sequence<T, Alloc, EnableSSO>>
    : std::bool_constant<is_trivially_relocatable_v<Alloc>> {};


}  // namespace parlay

namespace std {

// exchange the values of a and b
template<typename T, typename Allocator, bool EnableSSO>
inline void swap(parlay::sequence<T, Allocator, EnableSSO>& a, parlay::sequence<T, Allocator, EnableSSO>& b) {
  a.swap(b);
}

// compute a suitable hash value for a sequence
template<typename T, typename Allocator, bool EnableSSO>
struct hash<parlay::sequence<T, Allocator, EnableSSO>> {
  std::size_t operator()(parlay::sequence<T, Allocator, EnableSSO> const& s) const noexcept {
    size_t hash = 5381;
      for (size_t i = 0; i < s.size(); i++) {
        hash = ((hash << 5) + hash) + parlay::hash<T>{}(s[i]);
      }
      return hash;
  }
};

}  // namespace std

#endif  // PARLAY_SEQUENCE_H_
