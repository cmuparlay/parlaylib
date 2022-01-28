# Sequence

<small>**Usage: `#include <parlay/sequence.h>`**</small>

```c++
template <
    typename T,
    typename Allocator = parlay::allocator<T>,
    bool EnableSSO = false
> class sequence
```

A **sequence** is a parallel version of `std::vector`. It supports the same operations, but performs all initialization, updates, and destruction in parallel. Sequences satisfiy the range concept. Like `std::vector`, it stores all elements contiguously, and hence it is well defined to operate on pointers to ranges of elements of the sequence. It optionally supports small-size optimization, where sequences of trivial types that fit inside the sequence object will be stored inline without performing any heap allocations. A convenience alias, `short_sequence<T, Allocator>`, is provided, which is equivalent to `sequence<T, Allocator, true>`.

## Reference

- [Template parameters](#template-parameters)
- [Constructors](#constructors)
- [Member types](#member-types)
- [Member functions](#member-functions)
- [Factories](#factories)
- [Non-member functions](#non-member-functions)


### Template parameters

* **T** is the type of the elements of the sequence. In general, elements of a sequence should be move constructible, but immobile types (e.g. `std::atomic`) can be used provided that no operations that could trigger a reallocation (e.g. push_back, resize, etc) are used. T must be *Erasable*, i.e. destructible.
* **Allocator** is the allocator used to allocate/deallocate memory for the sequence. The `value_type` of the allocator must be `T`. By default, `parlay::allocator<T>` is used. To switch the default to `std::allocator<T>`, add the compile definition `PARLAY_USE_STD_ALLOC`
* **EnableSSO** should be true to enable small-size optimization. This will only have an effect for trivial types that fit inside the sequence object (typically sequences of elements totalling at most 15 bytes).

### Constructors

```c++
sequence()
sequence(const sequence& s)
sequence(sequence&& rv) noexcept
```

Constructs an empty sequence, a copy of the given sequence, or moves the given sequence respectively.

```c++
sequence(size_t n)
sequence(size_t n, const value_type& t) 
```

Constructs a sequence consisting of `n` value initialized elements of type `T`, or of `n` copies of the value `t`.

```c++
template<typename _Iterator> sequence(_Iterator i, _Iterator j)
sequence(std::initializer_list<value_type> l)
```

Constructs a sequence consisting of copies of the elements of the iterator range `(i, j]`, or copies of the elements of the initializer list `l`.

### Member types

Type | Definition
---|---
`value_type` | Same as `T`
`reference` | `T&`
`const_reference` | `const T&`
`difference_type` | `std::ptrdiff_t`
`size_type` | `size_t`
`pointer` | `T*`
`const_pointer` | `const T*`
`iterator` | `T*` (Satisfies random access iterator)
`const_iterator` | Equivalent to `const iterator`
`reverse_iterator` | `std::reverse_iterator<iterator>`
`const_reverse_iterator` | Equivalent to `std::reverse_iterator<const_iterator>`
`view_type` | `slice<iterator, iterator>`
`const_view_type` | `slice<const_iterator, const_iterator>`


### Member functions

**Iterators**

Function | Description
---|---
`iterator begin()` | Iterator to the beginning of the sequence
`iterator end()` | Iterator to the end of the sequence
`const_iterator cbegin()` | Constant iterator to the beginning of the sequence
`const_iterator cend()` | Constant iterator to the end of the sequence
`reverse_iterator rbegin()` | Reverse iterator to the end of the sequence
`reverse_iterator rend()` | Reverse iterator to the beginning of the sequence
`const_reverse_iterator crbegin()` | Constant reverse iterator to the end of the sequence
`const_reverse_iterator crend()` | Constant reverse iterator to the beginning of the sequence

The functions for each iterator type also have corresponding const overloads.

**Size**

Function | Description
---|---
`size_type size()` | Returns the length of the sequence
`size_type max_size()` | Returns the maximum possible length of a sequence
`bool empty()` | Returns true if the sequence is empty
`void reserve(size_t n)` | Request that the capacity of the sequence be at least `n`
`size_type capacity()` | Returns the current capacity of the sequence

**Element access**

Function | Description
---|---
`value_type* data()` | Returns a pointer to the underlying raw data buffer
`value_type& operator[](size_type i)` | Returns a reference to the element at position `i`
`value_type& at(size_type i)` | Same as `operator[]`, but throws an exception if `i` is out of range
`value_type& front()` | Returns a reference to the first element of the sequence
`value_type& back()` |  Returns a reference to the last element of the sequence

Each of these also has a corresponding const overload.

**Bulk element access**

Function | Description
---|---
`view_type head(iterator p)` | Returns a slice corresponding to all elements of the sequence before `p`
`view_type head(size_type len)` | Returns a slice corresponding to the first `len` elements of the sequence
`view_type tail(iterator p)` | Returns a slice corresponding to all elements from `p` onwards
`view_type tail(size_type len)` | Returns a slice corresponding to the final `len` elements of the sequence
`view_type cut(size_type begin, size_type end)` | Returns a slice corresponding to the elements at between indices `begin` and `end`

If the sequence is mutable, the resulting slice is also mutable. Each of these also has a corresponding const overload.

**Insertion**

Function | Description
---|---
`template<typename... Args> iterator emplace(iterator p, Args&&... args) ` | Constructs an element of type `T` from the given arguments and inserts it at the position in the sequence pointed to by `p`
`template<typename... Args> iterator emplace_back(Args&&... args)` | Constructs an element of type `T` from the given arguments and appends it to the end of the sequence
`iterator push_back(const value_type& v)` | Appends a copy of the value `v` to the end of the sequence
`iterator push_back(value_type&& v)` | Moves the value `v` to append it to the end of the sequence
`iterator insert(iterator p, const value_type& v)` | Inserts a copy of the value `v` at the position in the sequence pointed to by `p`
`iterator insert(iterator p, value_type&& v)` | Moves the value `v` to insert it at the position in the sequence pointed to by `p`

These functions all return an iterator to the newly inserted value.

**Resetting**

Function | Description
---|---
`template<typename _Iterator> void assign(_Iterator i, _Iterator j)` | Clears the sequence and assigns it to contain copies of the elements in the iterator range `[i, j)`
`void assign(size_type n, const value_type& v)` | Clears the sequence and assigns it to contain `n` copies of the value `v`
`void assign(std::initializer_list<value_type> l)` | Clears the sequence and assigns it to contain copies of the elements in the initializer list `l`
`template<typename R> void assign(R&& r)` | Clears the sequence and assigns it to contain copies of the elements in the iterable range `r`
`void resize(size_type new_size)` | Resize the sequence to `new_size`. If `new_size` is larger than the current size, the sequence is extended with default constructed elements of type `T`.
`void resize(size_type new_size, const value_type& v)` | Resize the sequence to `new_size`. If `new_size` is larger than the current size, the sequence is extended with copies of the value `v`.


**Bulk insertion**

Function | Description
---|---
`iterator append(size_t n, const value_type& v)` | Appends `n` copies of the value `v` to the end of the sequence
`template<typename _Iterator> iterator append(_Iterator first, _Iterator last)` | Append copies of the elements contained in the iterator range `[first, last)` to the end of the sequence
`template<typename R> iterator append(R&& r)` | Appends copies of the elements of the range `r` to the end of the sequence. If the given range is an rvalue, its contents will be moved
`iterator insert(iterator p, size_t n, const value_type& v)` | Inserts `n` copies of the value `v` at the position in the sequence pointed to by `p`
`template<typename R> iterator insert(iterator p, R&& r)` | Inserts copies of the elements of the iterable range `r` at the position in the sequence pointed to by `p`
`template<typename _Iterator> iterator insert(iterator p, _Iterator i, _Iterator j)` | Inserts copies of the elements contained in the iterator range `[first, last)` at the position in the sequence pointed to by `p`
`iterator insert(iterator p, std::initializer_list<value_type> l)` | Insert copies of the contents of the initializer list `l` at the position in the sequence pointed to by `p`

These functions all return an iterator to the beginning of the range of the newly inserted elements.

**Deletion**

Function | Description
---|---
`iterator erase(iterator q)` | Removes the element at the position in the sequence pointed to by `q`. Returns a pointer to the element that is now at the position that was previously pointed to by `q`
`void pop_back()` | Removes the last element of the sequence
`void clear()` | Removes all elements of the sequence

**Bulk deletion**

Function | Description
---|---
`iterator erase(iterator q1, iterator q2)` | Removes all elements in the iterator range `[q1, q2)` and returns a pointer to the element now at the position that was previously pointed to by `q1`
`sequence pop_tail(iterator p)` | Removes all elements of the sequence from that pointed to by `p` onwards and returns a new sequence consisting of the removed elements
`sequence pop_tail(size_type len)` | Removes the last `len` elements from the sequence and returns a new sequence consisting of the removed elements

**Miscelaneous**

Function | Description
---|---
`bool operator==(const sequence& other)` | Compare two sequences for equality
`bool operator!=(const sequence& b)` | Compare two sequences for inequality
`void swap(sequence& b)` | Swap the contents the sequence with another
`sequence& operator=(sequence b)` | Copy and move assignment
`sequence& operator=(std::initializer_list<value_type> l)` | Assignment from an initializer list

### Factories

```c++
sequence<T, Allocator, EnableSSO>::uninitialized(size_t n)
```

Returns a sequence of size `n` containing uninitialized memory. If `T` is a non-trivial type, memory must be initialized before any operation that could resize, delete, or otherwise destroy any element of the sequence, or this will lead to undefined behaviour. Use with caution.

```c++
sequence<T, Allocator, EnableSSO>::from_function(size_t n, F&& f)
```

Returns a sequence consisting of elements of type `T` constructed from `f(0), f(1), ..., f(n-1)`.

### Non-member functions

```c++
template<typename R>
auto to_sequence(R&& r) -> sequence<range_value_type_t<R>>
```
```c++
template<typename R>
auto to_short_sequence(R&& r) -> short_sequence<range_value_type_t<R>>
```

Return a sequence or short sequence respectively, consisting of copies of the elements of the iterable range `r`.

