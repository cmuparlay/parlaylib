# Slice

<small>**Usage: `#include <parlay/slice.h>`**</small>

```c++
template <
    typename It,
    typename S
> struct slice
```

A **slice** is a non-owning view of a random-access iterator range. It represents a pair of iterators, and allows for conveniently traversing and accessing the elements of the corresponding iterator range.

## Reference

- [Template parameters](#template-parameters)
- [Constructors](#constructors)
- [Member types](#member-types)
- [Member functions](#member-functions)
- [Factories](#factories)
- [Non-member functions](#non-member-functions)


### Template parameters

* **It** is the type of the iterator representing the iterator range
* **S** is the type of the sentinel that represents the end of the iterator range. This is usually the same as **It** for most ranges, but need not necessarily be.

### Constructors

```c++
slice(iterator s, sentinel e);
```

Construct a slice corresponding to the iterator range `[s, e)`.

```c++
slice(const slice<It,S>&)
slice<It,S>& operator=(const slice<It,S>&);
```

The copy and move constructors simply copy or move the underlying iterators respectively.

### Member types

Type | Definition
---|---
`iterator` | `It`
`sentinel` | `S`
`value_type` | `std::iterator_traits<iterator>::value_type`
`reference` | `std::iterator_traits<iterator>::reference`

### Member functions

Function | Description
---|---
`reference operator[](size_t i) const` | Return a reference to the i'th element of the range
`size_t size() const` | Return the number of elements in the range
`slice<It, It> cut(size_t ss, size_t ee) const` | Return a slice corresponding to the elements from positions `ss` to `ee` in the range
`iterator begin() const` | Return the iterator to the beginning of the range
`sentinel end() const` | Return the sentinel iterator that denotes the end of the range

### Factories

```c++
template<typename It, typename S>
slice<It, S> make_slice(It it, S s);
    
template<typename R>
slice<range_iterator_type_t<R>, range_sentinel_type_t<R>> make_slice(R&& r);
```

Returns a slice that views the iterator range `[it, s)`, or the given random-access range `r`.
    
### Non-member functions

Function | Description
---|---
`bool operator==(slice<It, S> s1, slice<It, S> s2) const` | Returns true if the given slices refer to the same iterator range

Note that `operator==` is only well defined when the two slices refer to iterator ranges that are subranges of the same range. Comparing iterators from different containers is undefined behavior.

