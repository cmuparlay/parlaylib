// Delayed sequences are random access iterator ranges
// that generate their elements on demand. Their memory
// requirement is therefore at most that of the function
// object that generates the range. Unlike most common
// iterators, dereferencing them can possibly result in
// a prvalue (temporary) rather than a reference.
//
// A delayed sequence is defined by a maximum index and a
// function object. The recommended way to construct
// a delayed sequence is by using the delayed_tabulate
// function. Example:
//
// auto s = parlay::delayed_tabulate(1000,
//            [](size_t i) { return i*i; });
//
// By default, the reference type of the delayed sequence
// is the return type of the function F and the value type
// is std::remove_cv_t<std::remove_reference_t<reference>>.
// These can be customized by providing template arguments
// to delayed_tabulate.
//

#ifndef PARLAY_DELAYED_SEQUENCE_H_
#define PARLAY_DELAYED_SEQUENCE_H_

#include <cassert>
#include <cstddef>

#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "utilities.h"

namespace parlay {

template<typename T, typename V, typename F>
class delayed_sequence;

// Factory function that can infer the type of the function
//
// Deprecated. Prefer to use delayed_tabulate instead.
template <typename T, typename F>
delayed_sequence<T, std::remove_cv_t<std::remove_reference_t<T>>, F> delayed_seq (size_t, F);

// A delayed sequence is an iterator range that generates its
// elements on demand.
//
// T is the reference_type of the sequence, i.e. the type returned
// by the delayed sequence. V is the value_type of the sequence.
// Often, these are the same, e.g., when the delayed sequence
// returns a prvalue (temporary). They should differ if the
// delayed sequence is going to return a reference, or a type
// with reference-semantics (e.g., a tuple of references), in
// which case value_type should be a type with corresponding value
// semantics.
//
template<typename T, typename V, typename F>
class delayed_sequence {
  static_assert(std::is_invocable_r_v<T, const F&, size_t>);

 public:
  
  // Types exposed by standard containers (although
  // delayed_sequence is not itself a container).
  //
  // Note that, counterintuitively, reference is not necessarily
  // a reference type, but rather, it refers to the type returned
  // by dereferencing the iterator. For a delayed sequence, this
  // could be a value type since elements are generated on demand.
  using value_type = V;
  using reference = T;
  using const_reference = std::add_const_t<T>;
  class iterator;
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = reverse_iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
 
  // ------------------------------------------------
  //    Internal iterator for a delayed sequence
  // ------------------------------------------------
  class iterator {
   public:

    // Iterator traits
    using iterator_category = std::random_access_iterator_tag;
    using value_type = V;
    using difference_type = std::ptrdiff_t;
    using pointer = typename std::add_pointer<T>::type;
    using reference = T;

    friend class delayed_sequence<T, V, F>;

    // ----- Requirements for vanilla iterator ----

    // Copy constructor and copy assignment
    iterator(const iterator& other) = default;
    iterator& operator=(const iterator&) = default;

    // Destructor
    ~iterator() = default;

    // Forward iteration
    iterator& operator++() {
      assert(index < parent->last);
      index++;
      return *this;
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

    // Dereference
    T operator*() const { return parent->f(index); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // ---- Requirements for input iterator ----

    iterator operator++(int) {
      assert(index < parent->last);
      auto tmp = *this;
      index++;
      return tmp;
    }

    bool operator==(const iterator& other) const {
      return index == other.index;
    }

    bool operator!=(const iterator& other) const {
      return index != other.index;
    }

    // ---- Requirements for forward iterator ----

    // Value-initialized (default constructed) iterators should
    // compare equal, but should not be dereferenced or incremented
    iterator() : parent(nullptr), index(0) {}

    // ---- Requirements for bidirectional iterator ----

    iterator& operator--() {
      assert(index > parent->first);
      index--;
      return *this;
    }

    iterator operator--(int) {
      assert(index > parent->first);
      auto tmp = *this;
      index--;
      return tmp;
    }

    // ---- Requirements for random access iterator ----

    bool operator<(const iterator& other) const {
      return index < other.index;
    }

    bool operator>(const iterator& other) const {
      return index > other.index;
    }

    bool operator<=(const iterator& other) const {
      return index <= other.index;
    }

    bool operator>=(const iterator& other) const {
      return index >= other.index;
    }

    iterator& operator+=(size_t delta) {
      assert(index + delta <= parent->last);
      index += delta;
      return *this;
    }

    iterator operator+(size_t delta) const {
      assert(index + delta <= parent->last);
      return iterator(parent, index + delta);
    }

    iterator& operator-=(size_t delta) {
      assert(index - delta >= parent->first);
      index -= delta;
      return *this;
    }

    iterator operator-(size_t delta) const {
      assert(index - delta >= parent->first);
      return iterator(parent, index - delta);
    }

    std::ptrdiff_t operator-(const iterator& other) const {
      return static_cast<std::ptrdiff_t>(index)
        - static_cast<std::ptrdiff_t>(other.index);
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

    T operator[](size_t i) const { return parent->f(index + i); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

   private:

    iterator(const delayed_sequence* _parent, size_t _index)
      : parent(_parent), index(_index) { }
   
    const delayed_sequence<T, V, F>* parent;
    size_t index;
  };
  
  // ----------- Constructors and Factories ---------------

  static auto constant(size_t n, T value) {
    return delayed_seq<T>(n,
      [val = std::move(value)](size_t) { return val; });
  }

  delayed_sequence(size_t n, F _f)
    : first(0), last(n), f(std::move(_f)) { }

  delayed_sequence(size_t _first, size_t _last, F _f)
    : first(_first), last(_last), f(std::move(_f)) { }
    
  // Default copy & move, constructor and assignment
  delayed_sequence(const delayed_sequence<T, V, F>&) = default;
  delayed_sequence(delayed_sequence<T, V, F>&&) = default;                          // NOLINT: A default move constructor is noexcept whenever possible
  delayed_sequence<T, V, F>& operator=(const delayed_sequence<T, V, F>&) = default;
  delayed_sequence<T, V, F>& operator=(delayed_sequence<T, V, F>&&) = default;      // NOLINT: A default move assignment is noexcept whenever possible

  // ---------------- Iterator Pairs --------------------

  iterator begin() const { return iterator(this, first); }
  iterator end() const { return iterator(this, last); }

  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }
  
  reverse_iterator rbegin() const { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() const { return std::make_reverse_iterator(begin()); }
  
  const_reverse_iterator crbegin() const { return rbegin(); }
  const_reverse_iterator crend() const { return rend(); }
  
  // ---------------- Other operations --------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to *, possible loss of data
#endif

  // Subscript access
  T operator[](size_t i) const { return f(i); }

  // Subscript access with bounds checking
  T at(size_t i) const {
    if (i < first || i >= last) {
      throw_exception_or_terminate<std::out_of_range>("Delayed sequence access out of range at " + std::to_string(i) +
                                                      "for a sequence with bounds [" + std::to_string(first) + ", " +
                                                      std::to_string(last) + ")");
    }
    return f(i);
  }

  // Return the first element
  T front() const {
    assert(!empty());
    return f(first);
  }

  // Return the last element
  T back() const {
    assert(!empty());
    return f(last-1);
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  // Size
  [[nodiscard]] size_t size() const {
    assert(first <= last);
    return last - first;
  }
  
  // Is empty?
  [[nodiscard]] bool empty() const {
    return size() == 0;
  }
  
  // Swap this with another delayed sequence
  void swap(delayed_sequence<T, V, F>& other) {
    std::swap(*this, other);
  }
  
 private:
  size_t first, last;
  copyable_function_wrapper<F> f;
};

// Factory function that can infer the type of the function
//
// Deprecated. Prefer to use delayed_tabulate instead.
template <typename T, typename F>
delayed_sequence<T, std::remove_cv_t<std::remove_reference_t<T>>, F> delayed_seq (size_t n, F f) {
  return delayed_sequence<T, std::remove_cv_t<std::remove_reference_t<T>>, F>(n, std::move(f));
}

}  // namespace parlay

#endif  // PARLAY_DELAYED_SEQUENCE_H_
