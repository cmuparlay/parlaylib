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
- [Non-member functions](#non-member-functions)


### Template parameters

* **It** is the type of the iterator representing the iterator range
* **S** is the type of the sentinel that represents the end of the iterator range. This is usually the same as **It** for most ranges, but need not necessarily be.

### Constructors

```c++
slice(iterator s, sentinel e)
```

Construct a slice corresponding to the iterator range `[s, e)`.

```c++
slice(const slice<It,S>&)
slice<It,S>& operator=(const slice<It,S>&)
```

The copy and move constructors simply copy or move the underlying iterators respectively.

### Member types

Type | Definition
---|---
`iterator` | Equal to `It`
`sentinel` | Equal to `S`
`value_type` | Equal to `std::iterator_traits<iterator>::value_type`
`reference` | Equal to `std::iterator_traits<iterator>::reference`

### Member functions

Function | Description
---|---
`reference operator[](size_t i)` | Return a reference to the i'th element of the range
`size_t size()` | Return the number of elements in the range
`slice<It, It> cut(size_t ss, size_t ee)` | Return a slice corresponding to the elements from positions `ss` to `ee` in the range
`iterator begin()` | Return the iterator to the beginning of the range
`sentinel end()` | Return the sentinel iterator that denotes the end of the range

### Non-member functions

Function | Description
---|---
`bool operator==(slice<It, S> s1, slice<It, S> s2)` | Returns true if the given slices refer to the same iterator range

Note that `operator==` is only well defined when the two slices refer to iterator ranges that are subranges of a common range. Comparing iterators from different containers is undefined behavior.

