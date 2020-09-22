// Delayed sequences are random access iterator ranges
// that generate their elements on demand. Their memory
// requirement is therefore at most that of the function
// object that generates the range. Unlike most common
// iterators, dereferencing them results in a value type,
// not a reference. Consequently, they are immutable, and
// their iterators are always const iterators.
//
// A delayed sequence is defined by a range of indices
// and a function object. Example:
//
// auto s = parlay::delayed_seq<int>(0, 1000,
//            [](size_t i) { return i*i; });
//

#ifndef PARLAY_DELAYED_SEQUENCE_H_
#define PARLAY_DELAYED_SEQUENCE_H_

#include <cassert>
#include <cstddef>

#include <iterator>
#include <stdexcept>
#include <string>

namespace parlay {

template<typename T, typename F>
class delayed_sequence;

// Factory function that can infer the type of the function
template <typename T, typename F>
delayed_sequence<T, F> delayed_seq (size_t, F);

// A delayed sequence is an iterator range that generates its
// elements on demand.
template<typename T, typename F>
class delayed_sequence {
 public:
  
  // Types exposed by standard containers (although
  // delayed_sequence is not itself a container).
  //
  // Note that, counterintuitively, reference is not necessarily
  // a reference type, but rather, it refers to the type returned
  // by dereferencing the iterator. For a delayed sequence, this
  // must be a value type since elements are generated on demand.
  using value_type = T;
  using reference = T;
  using const_reference = T;
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
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = T;

    friend class delayed_sequence<T, F>;

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

    // Dereference by value
    T operator*() const { return parent->f(index); }
 
    // ---- Requirements for input iterator ----

    iterator operator++(int) const {
      assert(index < parent->last);
      return iterator(parent, index+1);
    }

    bool operator==(const iterator& other) const {
      return index == other.index;
    }

    bool operator!=(const iterator& other) const {
      return index != other.index;
    }

    // ---- Requirements for bidirectional iterator ----

    iterator& operator--() {
      assert(index > parent->first);
      index--;
      return *this;
    }

    iterator operator--(int) const {
      assert(index > parent->first);
      return iterator(parent, index-1);
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

    T operator[](size_t i) const { return parent->f(index + i); }
 
   private:

    iterator(const delayed_sequence* _parent, size_t _index)
      : parent(_parent), index(_index) { }
   
    const delayed_sequence<T, F>* parent;
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
  delayed_sequence(const delayed_sequence<T, F>&) = default;
  delayed_sequence(delayed_sequence<T, F>&&) = default;                           // NOLINT: A default move constructor is noexcept whenever possible
  delayed_sequence<T, F>& operator=(const delayed_sequence<T, F>&) = default;
  delayed_sequence<T, F>& operator=(delayed_sequence<T, F>&&) = default;          // NOLINT: A default move assignment is noexcept whenever possible

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
  
  // Subscript access
  T operator[](size_t i) const { return f(i); }
  
  // Subscript access with bounds checking
  T at(size_t i) const {
    if (i < first || i >= last) {
      throw std::out_of_range("Delayed sequence access out of"
                              "range at " + std::to_string(i) +
                              "for a sequence with bounds [" +
                              std::to_string(first) + ", " +
                              std::to_string(last) + ")");
    }
    return f(i);
  }
  
  // Size
  size_t size() const {
    assert(first <= last);
    return last - first;
  }
  
  // Is empty?
  bool empty() const {
    return size() == 0;
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
  
  // Swap this with another delayed sequence
  void swap(delayed_sequence<T, F>& other) {
    std::swap(*this, other);
  }
  
 private:
  size_t first, last;
  F f;
};

// Factory function that can infer the type of the function
template <typename T, typename F>
delayed_sequence<T, F> delayed_seq (size_t n, F f) {
  return delayed_sequence<T, F>(n, std::move(f));
}

}  // namespace parlay

#endif  // PARLAY_DELAYED_SEQUENCE_H_
