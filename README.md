
# ParlayLib - A Toolkit for Programming Parallel Algorithms on Shared-Memory Multicore Machines

[![Build Status](https://travis-ci.org/cmuparlay/parlaylib.svg?branch=master)](https://travis-ci.org/cmuparlay/parlaylib)
[![Build status](https://ci.appveyor.com/api/projects/status/2458wr1nbcusxhqb/branch/master?svg=true)](https://ci.appveyor.com/project/DanielLiamAnderson/parlaylib/branch/master)
[![codecov](https://codecov.io/gh/cmuparlay/parlaylib/branch/master/graph/badge.svg)](https://codecov.io/gh/cmuparlay/parlaylib)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)


ParlayLib is a C++ library for developing efficient parallel algorithms and software on shared-memory multicore machines. It provides additional tools and primitives that go beyond what is available in the C++ standard library, and simplifies the task of programming provably efficient and scalable parallel algorithms. It consists of a sequence data type (analogous to std::vector), many parallel routines and algorithms, a work-stealing scheduler to support nested parallelism, and a scalable memory allocator. It has been developed over a period of seven years and used in a variety of software including the [PBBS benchmark suite](http://www.cs.cmu.edu/~pbbs/benchmarks.html), the [Ligra](http://jshun.github.io/ligra/), [Julienne](https://dl.acm.org/doi/pdf/10.1145/3087556.3087580), and [Aspen](https://github.com/ldhulipala/aspen) graph processing frameworks, the [Graph Based Benchmark Suite](https://github.com/ParAlg/gbbs), and the [PAM](https://cmuparlay.github.io/PAMWeb/) library for parallel balanced binary search trees, and an implementation of the TPC-H benchmark suite.

Parlay is designed to be reasonably portable by being built upon mostly standards-compliant modern C++. It builds on [GCC](https://gcc.gnu.org/) and [Clang](https://clang.llvm.org/) on Linux, GCC and Apple Clang on OSX, and Microsoft Visual C++ ([MSVC](https://visualstudio.microsoft.com/vs/)) and [MinGW](http://www.mingw.org/) on Windows. It is also tested on GCC and Clang via Windows Subsystem for Linux ([WSL](https://docs.microsoft.com/en-us/windows/wsl/about)) and [Cygwin](https://www.cygwin.com/). Support beyond x86-64 has not yet been explored. We would warmly welcome contributions that seek to achieve this.

This documentation is a work in progress and is not yet fully complete.

**Contents**

- [Getting started](#getting-started)
    + [Installing and including via CMake](#installing-and-including-via-cmake)
    + [Installing and including manually](#installing-and-including-manually)
    + [The old fashioned way](#the-old-fashioned-way)
- [Developer documentation](#developer-documentation)
- [Using Parlay with Cilk, OpenMP, or TBB](#using-parlay-with-cilk-openmp-or-tbb)
- [Features](#features)
  * [Parallel scheduling](#parallel-scheduling)
  * [Data structures](#data-structures)
    + [The Range concept](#the-range-concept)
    + [Slice](#slice)
    + [Sequence](#sequence)
    + [Delayed Sequence](#delayed-sequence)
    + [Phase-concurrent Hashtable](#phase-concurrent-hashtable)
  * [Parallel algorithms](#parallel-algorithms)
    + [Tabulate](#tabulate)
    + [Map](#map)
    + [Copy](#copy)
    + [Reduce](#reduce)
    + [Scan](#scan)
    + [Pack](#pack)
    + [Filter](#filter)
    + [Merge](#merge)
    + [Histogram](#histogram)
    + [Sort](#sort)
    + [Integer Sort](#integer-sort)
    + [For each](#for-each)
    + [Count](#count)
    + [All of, any of, none of](#all-of-any-of-none-of)
    + [Find](#find)
    + [Adjacent find](#adjacent-find)
    + [Mismatch](#mismatch)
    + [Search](#search)
    + [Find end](#find-end)
    + [Equal](#equal)
    + [Lexicographical compare](#lexicographical-compare)
    + [Unique](#unique)
    + [Min and max element](#min-and-max-element)
    + [Reverse](#reverse)
    + [Rotate](#rotate)
    + [Is sorted](#is-sorted)
    + [Is partitioned](#is-partitioned)
    + [Remove](#remove)
    + [Iota](#iota)
    + [Flatten](#flatten)
    + [Tokens](#tokens)
    + [Split](#split)
  * [I/O, Parsing, and Formatting](#io-parsing-and-formatting)
    + [Reading and writing files](#reading-and-writing-files)
    + [Writing character sequences to streams](#writing-character-sequences-to-streams)
    + [Memory-mapped files](#memory-mapped-files)
    + [Parsing](#parsing)
  * [Memory Allocator](#memory-allocator)
   



# Getting started

ParlayLib is a lightweight header-only library, so it is easy to integrate into your new or existing projects. There are many ways to acomplish this.

### Installing and including via CMake

Parlay comes configured to work with CMake, so if CMake is your preferred choice of build system, integrating Parlay is straightforward. To install Parlay, create a build directory (any name is fine) in the Parlay root directory, and from that build directory, configure cmake by running `cmake ..`. If you wish to change the installation location (the default is usually /usr/local/) then you can add the option `-DCMAKE_INSTALL_PREFIX:PATH=/your/installation/path`. After configuring, run `cmake --build . --target install` (or just `make install` for short if you happen to be using Make as the underlying build tool), possibly with `sudo` if necessary, to install Parlay.

Now that Parlay is installed, you can locate and include it via CMake by adding the following to the CMakeLists.txt of your project

```
find_package(Threads REQUIRED)
find_package(Parlay CONFIG REQUIRED)
```

You can then link Parlay to any of your targets by writing

```
target_link_libraries(my_target PRIVATE Parlay::parlay)
```

You are now ready to go!

### Installing and including manually

If you use a different build system, you have the option of installing Parlay via CMake (see above), or installing it manually, and then including it manually or via your preferred build system. If you install Parlay to a location that is not in your compiler's include path, you should tell your compiler where to find it by adding an include flag like so.

```
-I/path/to/parlay/include/location
```

To ensure that Parlay builds correctly, make sure that you are compiling with C++17 enabled by adding `-std=c++17` or equivalent (e.g. `/std:c++17` on Microsoft) to your compiler options. You'll also need threading support, which is usually achieved via one of the flags `-pthread`, `-pthreads`, or your operating system's equivalent.

### The old fashioned way

If fancy build systems are not your thing, the tried and true way to include Parlay in your project is to simply copy the source code of Parlay directly into your own project. Since the library is header only, this should work out of the box, assuming you add any required flags (see above). One possible way to do this while still enabling updates to ParlayLib is to include it as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) of your project's Git repository.

# Developer documentation

If you are interested in contributing to Parlay, the following pages describe useful information about our testing and benchmarking setups. If you just want to use Parlay in your own projects, these links are not relevant to you.

* [Static analysis](./analysis/README.md)
* [Unit tests](./test/README.md)
* [Benchmarks](./benchmark/README.md)

# Using Parlay with Cilk, OpenMP, or TBB

If you're already using Cilk, OpenMP, or Thread Building Blocks, and just want to use Parlay's algorithms without its parallel scheduler, that is easy to do. When building your program, simply add the appropriate compile definition as below.

```
-DPARLAY_CILK
-DPARLAY_OPENMP
-DPARLAY_TBB
```

Parlay will then use the specified framework's parallel operations to support its algorithms instead of its own scheduler.

# Features

Parlay includes a number of pieces that can be used together or individually. At a high level, we have

* A library of useful parallel primitives
* Parallel data structures
* A parallel scheduler
* A scalable memory allocator


## Parallel scheduling

<small>**Usage: `#include <parlay/parallel.h>`**</small>

Parlay offers an interface for fork-join parallelism in the form of a fork operation, and a parallel for loop.

**Binary forking**

The primitive `par_do` takes two function objects and evaluates them in parallel. For exampe, we can write a recursive sum function like so, where the left and right halves are evaluated in parallel.

```c++
template<typename Iterator>
auto sum(Iterator first, Iterator last) {
  if (first == last - 1) {
    return *first;
  }
  else {
    auto mid = first + std::distance(first, last) / 2;
    int left_sum, right_sum;
	parlay::par_do(
	  [&]() { left_sum = sum(first, mid); },
	  [&]() { right_sum = sum(mid, last); }
	)
	return left_sum + right_sum;
  }
}
```

**Parallel for loops**

A parallel for loop takes a start and end value, and a function object, and evaluates the function object at every value in [start, end).

```c++
parlay::sequence<int> a(100000);
parlay::parallel_for(0, 100000, [&](size_t i) {
  a[i] = i;
});
```

## Data structures

### The Range concept

<small>**Usage: `#include <parlay/range.h>`**</small>

Many of Parlays primitive are designed around the *range* concept. Essentially, a range in Parlay is any type that supports `std::begin(r)` and `std::end(r)`, such that `std::begin(r)` returns a random access iterator, and ``std::end(r) - std::begin(r)`` denotes the size of the range. In other words, it is any type that can be used as a finite-length random access sequence. It is meant to be a close approximation to the C++20 standarized concepts `std::ranges::random_access_range && std::ranges::sized_range` The concept is satisfied by `std::vector`, and by Parlays own `parlay::sequence`, and many other types.

If compiled with a recent compiler that supports C++ concepts, Parlay will check that ranges used in its primitives satisfy these requirements at compile time.

### Slice

<small>**Usage: `#include <parlay/slice.h>`**</small>

```c++
template <
    typename It,
    typename S
> struct slice
```

A **slice** is a non-owning view of a iterator range. It represents a pair of iterators, and allows for conveniently traversing and accessing the elements of the corresponding iterator range.

#### Template parameters

* **It** is the type of the iterator representing the iterator range
* **S** is the type of the sentinel that represents the end of the iterator range. This is almost always the same as **It**, but need not necessarily be,

#### Constructors

```c++
slice(iterator s, sentinel e)
```

Construct a slice corresponding to the iterator range `[s, e)`.

```c++
slice(const slice<It,S>&)
slice<It,S>& operator=(const slice<It,S>&)
```

The copy and move constructors simply copy or move the underlying iterators respectively.

#### Member types

Type | Definition
---|---
`iterator` | Equal to `It`
`sentinel` | Equal to `S`
`value_type` | Equal to `std::iterator_traits<iterator>::value_type`
`reference` | Equal to `std::iterator_traits<iterator>::reference`

#### Member functions

Function | Description
---|---
`reference operator[](size_t i)` | Return a reference to the i'th element of the range
`size_t size()` | Return the number of elements in the range
`slice<It, It> cut(size_t ss, size_t ee)` | Return a slice corresponding to the elements from positions `ss` to `ee` in the range
`iterator begin()` | Return the iterator to the beginning of the range
`sentinel end()` | Return the sentinel iterator that denotes the end of the range

#### Non-member functions

Function | Description
---|---
`bool operator==(slice<It, S> s1, slice<It, S> s2)` | Returns true if the given slices refer to the same iterator range

Note that `operator==` is only well defined when the two slices refer to iterator ranges that are subranges of a common range. Comparing iterators from different containers is undefined behavior.

### Sequence

<small>**Usage: `#include <parlay/sequence.h>`**</small>

```c++
template <
    typename T,
    typename Allocator = parlay::allocator<T>,
    bool EnableSSO = false
> class sequence
```

A **sequence** is a parallel version of `std::vector`. It supports the same operations, but performs all initialization, updates, and destruction in parallel. Sequences satisfiy the range concept. Like `std::vector`, it stores all elements contiguously, and hence it is well defined to operate on pointers to ranges of elements of the sequence. It optionally supports small-size optimization, where sequences of trivial types that fit inside the sequence object will be stored inline without performing any heap allocations. A convenience alias, `short_sequence<T, Allocator>`, is provided, which is equivalent to `sequence<T, Allocator, true>`.

#### Template parameters

* **T** is the type of the elements of the sequence. In general, elements of a sequence should be move constructible, but immobile types (e.g. `std::atomic`) can be used provided that no operations that could trigger a reallocation (e.g. push_back, resize, etc) are used. T must be *Erasable*, i.e. destructible.
* **Allocator** is the allocator used to allocate/deallocate memory for the sequence. The `value_type` of the allocator must be `T`. By default, `parlay::allocator<T>` is used. To switch the default to `std::allocator<T>`, add the compile definition `PARLAY_USE_STD_ALLOC`
* **EnableSSO** should be true to enable small-size optimization. This will only have an effect for trivial types that fit inside the sequence object (typically sequences of elements totalling at most 15 bytes).

#### Constructors

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

#### Member types

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


#### Member functions

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

#### Factories

```c++
sequence<T, Allocator, EnableSSO>::uninitialized(size_t n)
```

Returns a sequence of size `n` containing uninitialized memory. If `T` is a non-trivial type, memory must be initialized before any operation that could resize, delete, or otherwise destroy any element of the sequence, or this will lead to undefined behaviour. Use with caution.

```c++
sequence<T, Allocator, EnableSSO>::from_function(size_t n, F&& f)
```

Returns a sequence consisting of elements of type `T` constructed from `f(0), f(1), ..., f(n-1)`.

#### Non-member functions

```c++
template<typename R>
auto to_sequence(R&& r) -> sequence<range_value_type_t<R>>
```
```c++
template<typename R>
auto to_short_sequence(R&& r) -> short_sequence<range_value_type_t<R>>
```

Return a sequence or short sequence respectively, consisting of copies of the elements of the iterable range `r`.

### Delayed Sequence

<small>**Usage: `#include <parlay/delayed_sequence.h>`**</small>

```c++
template<
  typename T,
  typename F
> class delayed_sequence;
```

A delayed sequence is a lazy functional sequence that generates its elements on demand, rather than storing them in memory. A delayed sequence satisfies the range concept. The easiest way to construct a delayed sequence is to use the `delayed_tabulate` function (see below), since this avoids the need to explicitly specify the template parameters.

**Example**

```c++
// A sequence consisting of the first 1000 odd integers
auto seq = parlay::delayed_seq<int>(1000, [](size_t i) {
  return 2*i + 1;
});
```

#### Template parameters

* **T** is the type of the elements generated by the sequence
* **F** is the type of the function object that generates the elements of the sequence. It should be a function of type `T(size_t)`, i.e. a function that maps indices of type `size_t` to elements of type `T`.

#### Constructors

```c++
delayed_sequence(size_t n, F _f)
delayed_sequence(size_t _first, size_t _last, F _f)
```

Constructs a delayed sequence that generates elements from the given function `_f`. Given `n`, the sequence consists of the elements `f(0), ..., f(n-1)`. Otherwise, given `_first` and `_last`, the sequence consists of the elements `f(_first), ... f(_last-1)`.

```c++
delayed_sequence(const delayed_sequence&)
delayed_sequence(delayed_sequence&&) noexcept
delayed_sequence& operator=(const delayed_sequence&)
delayed_sequence& operator=(delayed_sequence&&) noexcept
```
The copy constructor and move constructor are viable provided that the underlying function object is copyable and movable respectively.

#### Member types

Type | Definition
---|---
`value_type` | Same as `T`
`reference` | Same as `T`
`const_reference` | Same as `T`
`iterator` | A constant random access iterator
`const_iterator` | Same as `iterator`
`reverse_iterator` | `std::reverse_iterator<iterator>`
`const_reverse_iterator` | Same as `reverse_iterator`
`difference_type` | `std::ptrdiff_t`
`size_type` | `size_t`

#### Member functions

**Iterators**

Function | Description
---|---
` iterator begin() ` | Iterator to the beginning of the sequence
` iterator end() ` |  Iterator to the end of the sequence
` const_iterator cbegin() ` |  Constant iterator to the beginning of the sequence
` const_iterator cend() ` |  Constant iterator to the end of the sequence
` reverse_iterator rbegin() ` |  Reverse iterator to the end of the sequence
` reverse_iterator rend() ` |  Reverse iterator to the beginning of the sequence
` const_reverse_iterator crbegin() ` | Constant reverse iterator to the end of the sequence
` const_reverse_iterator crend() ` |  Constant reverse iterator to the beginning of the sequence

**Element access**

Function | Description
---|---
`T operator[](size_t i)` | Generate the i'th element of the sequence
`T front()` | Generate the first element of the sequence
`T back()` | Generate the last element of the sequence

**Size**

Function | Description
---|---
`size_type size()` | Return the length of the sequence
`bool empty()` | Return true if the sequence is empty

**Miscelaneous**

Function | Description
---|---
`void swap(delayed_sequence& other)` | Swap this delayed sequence with another of the same type

### Phase-concurrent Hashtable

<small>**Usage: `#include <parlay/hash_table.h>`**</small>

A phase-concurrent hashtable allows for concurrent insertions, concurrent searches, and concurrent deletions, but does not permit mixing insertions, searching, and deletion concurrently.

```c++
parlay::hashtable<hash_numeric<int>> table;
table.insert(5);
auto val = table.find(5);  // Returns 5
table.deleteVal(5);
```

## Parallel algorithms

<small>**Usage: `#include <parlay/primitives.h>`**</small>

### Tabulate

```c++
template<typename UnaryOp>
auto tabulate(size_t n, UnaryOp&& f)
```

```c++
template<typename UnaryOp>
auto delayed_tabulate(size_t n, UnaryOp&& f)
```

**tabulate** takes an integer n and and a function f of integers, and produces a sequence consisting of f(0), f(1), ..., f(n-1). **delayed_tabulate** does the same but returns a delayed sequence instead.

### Map

```c++
template<parlay::Range R, typename UnaryOp>
auto map(R&& r, UnaryOp&& f)
```

```c++
template<parlay::Range R, typename UnaryOp>
auto delayed_map(R&& r, UnaryOp&& f)
```

**map** takes a range `r` and a function `f` from the value type of that range, and produces the sequence consisting of `f(r[0]), f(r[1]), f(r[2]), ...`.

**delayed_map** is the same as map, but the resulting sequence is a delayed sequence. Note that **delayed_map** forwards the range argument to the closure owned by the delayed sequence, so if `r` is an rvalue reference, it will be moved into and owned by the delayed sequence. If it is an lvalue reference, the delayed sequence will only keep a reference to `r`, so `r` must remain alive as long as the delayed sequence does.

### Copy

```c++
template<parlay::Range R_in, parlay::Range R_out>
void copy(const R_in& in, R_out& out)
```

**copy** takes a given range and copies its elements into another range

### Reduce

```c++
template<parlay::Range R>
auto reduce(const R& r)
```

```c++
template<parlay::Range R, typename Monoid>
auto reduce(const R& r, Monoid&& m)
```

**reduce** takes a range and returns the reduction with respect some associative binary operation (addition by default). The associative operation is specified by a monoid object which is an object that has a `.identity` field, and a binary operator `f`.

### Scan

```c++
template<parlay::Range R>
auto scan(const R& r)
```

```c++
template<parlay::Range R>
auto scan_inclusive(const R& r)
```

```c++
template<parlay::Range R>
auto scan_inplace(R&& r)
```

```c++
template<parlay::Range R>
auto scan_inclusive_inplace(R&& r)
```

```c++
template<parlay::Range R, typename Monoid>
auto scan(const R& r, Monoid&& m)
```

```c++
template<parlay::Range R, typename Monoid>
auto scan_inclusive(const R& r, Monoid&& m)
```

```c++
template<parlay::Range R, typename Monoid>
auto scan_inplace(R&& r, Monoid&& m)
```

```c++
template<parlay::Range R, typename Monoid>
auto scan_inclusive_inplace(R& r, Monoid&& m)
```

**scan** computes a scan (aka prefix sum) with respect to an associative binary operation (addition by default).  The associative operation is specified by a monoid object which is an object that has a `.identity` field, and a binary operator `f`. Scan returns a pair, consisting of the partial sums, and the total.

By default, scan considers prefix sums excluding the final element. There is also **scan_inclusive**, which is inclusive of the final element of each prefix. There are also inplace versions of each of these (**scan_inplace**, **scan_inclusive_inplace**), which write the sums into the input and return the total.

### Pack

```c++
template<parlay::Range R, parlay::Range BoolSeq>
auto pack(const R& r, const BoolSeq& b)
```

```c++
template<parlay::Range R_in, parlay::Range BoolSeq, parlay::Range R_out>
auto pack_into(const R_in& in, const BoolSeq& b, R_out& out)
```

```c++
template<parlay::Range BoolSeq>
auto pack_index(const BoolSeq& b) 
```

```c++
template<typename IndexType, parlay::Range BoolSeq>
auto pack_index(const BoolSeq& b) 
```

**pack** takes an input a range a boolean indicator sequence, and returns a new sequence consisting of the elements of the range such that the element in the corresponding position in the indicator sequence is true.

Similarly, **pack_into** does the same thing but writes the answer into an existing range. **pack_index** takes a range of elements that are convertible to bool, and returns a sequence of indices such that the elements at those positions convert to true.

### Filter

```c++
template<parlay::Range R, typename UnaryPred>
auto filter(const R& r, UnaryPred&& f) 
```

```c++
template<parlay::Range R_in, parlay::Range R_out, typename UnaryPred>
auto filter_into(const R_in& in, R_out& out, UnaryPred&& f)
```

**filter** takes a range and a unary operator, and returns a sequence consisting of the elements of the range for which the unary operator returns true. Alternatively, **filter_into** does the same thing but writes the output into the given range and returns the number of elements that were kept.

### Merge

```c++
template<parlay::Range R1, parlay::Range R2>
auto merge(const R1& r1, const R2& r2)
```

```c++
template<parlay::Range R1, parlay::Range R2, typename BinaryPred>
auto merge(const R1& r1, const R2& r2, BinaryPred pred)
```

**merge** returns a sequence consisting of the elements of `r1` and `r2` in sorted order, assuming
that `r1` and `r2` are already sorted. An optional binary predicate can be used to specify the comparison operation.

### [Experimental] Histogram


```c++
template<parlay::Range R>
auto histogram_by_key(R&& A)
```

```c++
template <typename sum_type = size_t, parlay::Range R, typename Hash, typename Equal>
auto histogram_by_key(R&& A, Hash hash, Equal equal)
```

```c++
template<typename Integer_, parlay::Range R>
auto histogram_by_index(const R& A, Integer_ m)
```

These functions are currently experimental and their interfaces may change soon.

**histogram_by_key** takes a range `A` and returns a sequence of key-value pairs, where the keys are the unique elements of `A`, and the values are the number of occurences of the corresponding key in `A`. Keys must be equality-comparable and hashable. The keys are not guaranteed to be in sorted order. Optionally, custom unary and binary operators can be supplied that specify how to hash and compare keys. By default, `std::hash` and `std::equal_to` are used. An optional template argument, `sum_type` allows the type of the counter values to be customized. 

**histogram_by_index** takes an integer-valued range `A` and a maximum value `m` and returns a sequence of length `m`, such that the `i`'th value of the sequence contains the number of occurences of `i` in `A`.
### Sort

```c++
template<parlay::Range R>
auto sort(const R& in)
```

```c++
template<parlay::Range R, typename Compare>
auto sort(const R& in, Compare&& comp)
```

```c++
template<parlay::Range R>
auto stable_sort(const R& in)
```

```c++
template<parlay::Range R, typename Compare>
auto stable_sort(const R& in, Compare&& comp)
```

```c++
template<parlay::Range R>
void sort_inplace(R&& in)
```

```c++
template<parlay::Range R, typename Compare>
void sort_inplace(R&& in, Compare&& comp)
```

```c++
template<parlay::Range R>
void stable_sort_inplace(R&& in)
```

```c++
template<parlay::Range R, typename Compare>
void stable_sort_inplace(R&& in, Compare&& comp)
```


**sort** takes a given range and outputs a sorted copy (unlike the standard library, sort is not inplace by default). **sort_inplace** can be used to sort a given range in place. **stable_sort** and **stable_sort_inplace** are the same but guarantee that equal elements maintain their original relative order. All of these functions can optionally take a custom comparator object, which is a binary operator that evaluates to true if the first of the given elements should compare less than the second.

### Integer Sort

```c++
template<parlay::Range R>
auto integer_sort(const R& in)
```

```c++
template<parlay::Range R, typename Key>
auto integer_sort(const R& in, Key&& key)
```

```c++
template<parlay::Range R>
void integer_sort_inplace(R&& in)
```

```c++
template<parlay::Range R, typename Key>
void integer_sort_inplace(R&& in, Key&& key)
```

**integer_sort** works just like sort, except that it is specialized to sort integer keys, and is significantly faster than ordinary sort. It can be used to sort ranges of integers, or ranges of arbitrary types if a unary operator is provided that can produce an integer key for any given element,

### For each

```c++
template <parlay::Range R, typename UnaryFunction>
void for_each(R&& r , UnaryFunction f)
```

**for_each** applies the given unary function to every element of the given range. The range may be constant, in which case the unary function should not attempt to modify it, or it may be mutable, in which case the function is allowed to modify it.

### Count

```c++
template <parlay::Range R, class T>
size_t count(const R& r, T const &value)
```

```c++
template <parlay::Range R, typename UnaryPredicate>
size_t count_if(const R& r, UnaryPredicate p)
```

**count** returns the number of elements in the given range that compare equal to the given value. **count_if** returns the number of elements that satisfy the predicate p.

### All of, any of, none of

```c++
template <parlay::Range R, typename UnaryPredicate>
bool all_of(const R& r, UnaryPredicate p)
```

```c++
template <parlay::Range R, typename UnaryPredicate>
bool any_of(const R& r, UnaryPredicate p)
```

```c++
template <parlay::Range R, typename UnaryPredicate>
bool none_of(const R& r, UnaryPredicate p)
```

**all_of**, **any_of**, and **none_of** return true if the given predicate is true for all, any, or none of the elements in the given range respectively.

### Find

```c++
template <parlay::Range R, typename T>
auto find(R&& r, T const &value)
```

```c++
template <parlay::Range R, typename UnaryPredicate>
auto find_if(R&& r, UnaryPredicate p)
```

```c++
template <parlay::Range R, typename UnaryPredicate>
auto find_if_not(R&& r, UnaryPredicate p)
```

```c++
template <parlay::Range R1, parlay::Range R2, typename BinaryPredicate>
auto find_first_of(R1&& r1, const R2& r2, BinaryPredicate p)
```

**find** returns an iterator to the first element in the given range that compares equal to the given value, or the end iterator if no such element exists. **find_if** returns the first element in the given range that satisfies the given predicate, or the end iterator if no such element exists. **find_if_not** similarly returns the first element that does not satisfy the given predicate, or the end iterator.

**find_first_of** returns an iterator to the first element in the range `r1` that compares equal to any of the elements in `r2`, or the end iterator of `r1` if no such element exists.

### Adjacent find

```c++
template <parlay::Range R>
auto adjacent_find(R&& r)
```

```c++
template <parlay::Range R, typename BinaryPred>
auto adjacent_find(R&& r, BinaryPred p)
```

**adjacent_find** returns an iterator to the first element in the given range that compares equal to the next element on its right, Optionally, a binary predicate can be supplied to dictate how two elements should compare equal.

### Mismatch

```c++
template <parlay::Range R1, parlay::Range R2>
size_t mismatch(R1&& r1, R2&& r2)
```

```c++
template <parlay::Range R1, parlay::Range R2, typename BinaryPred>
auto mismatch(R1&& r1, R2&& r2, BinaryPred p)
```

**mismatch** returns a pair of iterators corresponding to the first occurrence in which an element of `r1` is not equal to the element of `r2` in the same position. If no such occurrence exists, returns a pair containing the end iterator of `r1` and an iterator pointing to the corresponding position in `r2`. Optionally, a binary predicate can be supplied to dictate how two elements should compare equal.

### Search

```c++
template <parlay::Range R1, parlay::Range R2>
size_t search(R1&& r1, const R2& r2)
```

```c++
template <parlay::Range R1, parlay::Range R2, typename BinaryPred>
auto search(R1&& r1, const R2& r2, BinaryPred pred)
```

**search** returns an iterator to the beginning of the first occurrence of the range `r2` in `r1`, or the end iterator of `r1` if no such occurrence exists. Optionally, a binary predicate can be given to specify how two elements should compare equal.

### Find end

```c++
template <parlay::Range R1, parlay::Range R2>
auto find_end(R1&& r1, const R2& r2)
```

```c++
template <parlay::Range R1, parlay::Range R2, typename BinaryPred>
auto find_end(R1&& r1, const R2& r2, BinaryPred p)
```

**find_end** returns an iterator to the beginning of the last occurrence of the range `r2` in `r1`, or the end iterator of `r1` if no such occurrence exists. Optionally, a binary predicate can be given to specify how two elements should compare equal.

### Equal

```c++
template <parlay::Range R1, parlay::Range R2>
bool equal(const R1& r1, const R2& r2)
```

```c++
template <parlay::Range R1, parlay::Range R2, class BinaryPred>
bool equal(const R1& r1, const R2& r2, BinaryPred p)
```

**equal** returns true if the given ranges are equal, that is, they have the same size and all elements at corresponding positions compare equal. Optionally, a binary predicate can be given to specify how two elements should compare equal.

### Lexicographical compare

```c++
template <parlay::Range R1, parlay::Range R2, class Compare>
bool lexicographical_compare(const R1& r1, const R2& r2, Compare less)
```

**lexicographical_compare** returns true if the first range compares lexicographically less than the second range. A range is considered lexicographically less than another if it is a prefix of the other or the first mismatched element compares less than the corresponding element in the other range.

### Unique

```c++
template<parlay::Range R>
auto unique(const R& r)
```

```c++
template <parlay::Range R, typename BinaryPredicate>
auto unique(const R& r, BinaryPredicate eq)
```

**unique** returns a sequence consisting of the elements of the given range that do not compare equal to the element preceding them. All elements in the output sequence maintain their original relative order. An optional binary predicate can be given to specify how two elements should compare equal.

### Min and max element

```c++
template <parlay::Range R>
auto min_element(R&& r)
```

```c++
template <parlay::Range R, typename Compare>
auto min_element(R&& r, Compare comp)
```

```c++
template <parlay::Range R>
auto max_element(R&& r)
```

```c++
template <parlay::Range R, typename Compare>
auto max_element(R&& r, Compare comp)
```

```c++
template <parlay::Range R>
auto minmax_element(R&& r)
```

```c++
template <parlay::Range R, typename Compare>
auto minmax_element(R&& r, Compare comp)
```

**min_element** and **max_element** return a pointer to the minimum or maximum element in the given range respectively. In the case of duplicates, the leftmost element is always selected. **minmax_element** returns a pair consisting of iterators to both the minimum and maximum element. An optional binary predicate can be supplied to dictate how two elements should compare.

### Reverse

```c++
template <parlay::Range R>
auto reverse(const R& r)
```

```c++
template <parlay::Range R>
auto reverse_inplace(R&& r)
```

**reverse** returns a new sequence consisting of the elements of the given range in reverse order. **reverse_inplace** reverses the elements of the given range.

### Rotate

```c++
template <parlay::Range R>
auto rotate(const R& r, size_t t)
```

**rotate** returns a new sequence consisting of the elements of the given range cyclically shifted left by `t` positions.

### Is sorted

```c++
template <parlay::Range R>
bool is_sorted(const R& r)
```

```c++
template <parlay::Range R, typename Compare>
bool is_sorted(const R& r, Compare comp)
```

```c++
template <parlay::Range R>
auto is_sorted_until(const R& r)
```

```c++
template <parlay::Range R, typename Compare>
auto is_sorted_until(const R& r, Compare comp)
```

**is_sorted** returns true if the given range is sorted. **is_sorted_until** returns an iterator to the first element of the range that is out of order, or the end iterator if the range is sorted. An optional binary predicate can be supplied to dictate how two elements should compare.

### Is partitioned

```c++
template <parlay::Range R, typename UnaryPred>
bool is_partitioned(const R& r, UnaryPred f)
```

**is_partitioned** returns true if the given range is partitioned with respect to the given unary predicate. A range is partitioned with respect to the given predicate if all elements for which the predicate returns true precede all of those for which it returns false.

### Remove

```c++
template<parlay::Range R, typename T>
auto remove(const R& r, const T& v)
```

```c++
template <parlay::Range R, typename UnaryPred>
auto remove_if(const R& r, UnaryPred pred)
```

**remove** returns a sequence consisting of the elements of the range `r` with any occurrences of `v` omitted. **remove_if** returns a sequence consisting of the elements of the range `r` with any elements such that the given predicate returns true omitted.

### Iota

```c++
template <typename Index>
auto iota(Index n)
```

**iota** returns a sequence of the given template type consisting of the integers from `0` to `n-1`.

### Flatten

```c++
template <parlay::Range R>
auto flatten(const R& r)
```

**flatten** takes a range of ranges and returns a single sequence consisting of the concatenation of each of the underlying ranges.

### Tokens

```c++
template <parlay::Range R, typename UnaryPred = decltype(parlay::is_whitespace)>
sequence<sequence<char>> tokens(const R& r, UnaryPred is_space = parlay::is_whitespace)
```

```c++
template <parlay::Range R, typename UnaryOp,
          typename UnaryPred = decltype(parlay::is_whitespace)>
auto map_tokens(const R& r, UnaryOp f, UnaryPred is_space = parlay::is_whitespace)
```

**tokens** splits the given character sequence into "words", which are the maximal contiguous subsequences that do not contain any whitespace characters. Optionally, a custom criteria for determining the delimiters (whitespace by default) can be given. For example, to split a sequence at occurrences of commas, one could provide a value of `[](char c) { return c == ','; }` for `is_space`.

**map_tokens** splits the given character sequence into words in the same manner, but instead of returning a sequence of all the words, it applies the given unary function `f` to every token. More specifically, `f` must be a function object that can take a `slice` of `value_type` equal to `char`. If `f` is a void function, i.e. returns nothing, then `map_tokens` returns nothing. Otherwise, if `f` returns values of type `T`, the result of `map_tokens` is a sequence of type `T` consisting of the results of evaluating `f` on each token. For example, to compute a sequence that contains the lengths of all of the words in a given input sequence, one could write

```c++
auto word_sizes = parlay::map_tokens(my_char_sequence, [](auto token) { return token.size(); });
```

In essence, `map_tokens` is just equivalent to `parlay::map(parlay::tokens(r), f)`, but is more efficient, because it avoids copying the tokens into new memory, and instead, applies the function `f` directly to the tokens in the original sequence.

### Split

```c++
template <parlay::Range R, parlay::Range BoolRange>
auto split_at(const R& r, const BoolRange& flags)
```

```c++
template <parlay::Range R, parlay::Range BoolRange, typename UnaryOp>
auto map_split_at(const R& r, const BoolRange& flags, UnaryOp f)
```

**split_at** splits the given sequence into contiguous subsequences. Specifically, the subsequences are the maximal contiguous subsequences between positions such that the corresponding element in `flags` is true. This means that the number of subsequences is always one greater than the number of true flags. Also note that if there are adjacent true flags, the result can contain empty subsequences. The elements at positions corresponding to true flags are not themselves included in any subsequence.

**map_split_at** similarly splits the given sequence into contiguous subsequences, but instead of returning these subsequences, it applies the given unary function `f` to each one. Specifically, `f` must be a function object that can take a slice of the input sequence. If `f` is a void function (i.e. returns nothing), then `map_split_at` returns nothing. Otherwise, `map_split_at` returns a sequence consisting of the results of applying `f` to each of the contiguous subsequences of `r`.

`map_split_at` is essentially equivalent to `parlay::map(parlay::split_at(r, flags), f)`, but is more efficient because the subsequences do not have to be copied into new memory, but are instead acted upon by `f` in place.

## I/O, Parsing, and Formatting

<small>**Usage: `#include <parlay/io.h>`**</small>

Parlay includes some convenient tools for file input and output in terms of `parlay::sequence<char>`, as well as tools for converting to and from `parlay::sequence<char>` and primitive types.

### Reading and writing files

```c++
inline parlay::sequence<char> chars_from_file(const std::string& filename,
    bool null_terminate, size_t start=0, size_t end=0)
```
```c++
inline void chars_to_file(const parlay::sequence<char>& S,
    const std::string& filename)
```

**chars_from_file** reads the contents of a local file into a character sequence. If `null_terminate` is true, the sequence will be ended by a null terminator (`\0`) character. This is only required for compatability with C APIs and otherwise isn't necessary. To read a particular portion of a file rather than its entirety, the parameters `start` and `end` can specify the positions of the first and last character to read. If `start` or `end` is zero, the file is read from to beginning and/or to the end respectively.

**chars_to_file** writes the given character sequence to the local file with the given name. If a file with the given name already exists, it will be overwritten.

### Writing character sequences to streams

```c++
inline void chars_to_stream(const sequence<char>& S, std::ostream& os)
```
```c++
inline std::ostream& operator<<(std::ostream& os, const sequence<char>& s)
```

Character sequences can also be written to standard streams, i.e. types deriving from `std::ostream`. They support the standard `operator<<`, as well as a method **chars_to_stream**, which takes a character sequence and a stream, and writes the given characters to the stream.

### Memory-mapped files

```c++
class file_map
```

To support reading large files quickly and in parallel, possibly even files that do not fit in main memory, Parlay provides a wrapper type **file_map** for memory mapping. Memory mapping takes a given file on disk, and maps its contents to a range of addresses in virtual memory. Memory mapped files are much more efficient than sequential file reading because their contents can be read in parallel. The `file_map` type satisfies the range concept, and hence can be used in conjunction with all of Parlay's parallel algorithms.

#### Constructors

```c++
explicit file_map(const std::string& filename)
```

Memory maps the file with the given filename.

```c++
file_map(file_map&& other)
```

Objects of type `file_map` can be moved but not copied.


#### Member types

Type | Definition
---|---
`value_type` | The value type of the underlying file contents. Usually `char`
`reference` | Equal to `value_type&`
`const_reference` | Equal to `const value_type&`
`iterator` | An iterator to a range of elements of type `value_type`
`const_iterator` | Equal to `const iterator`
`pointer` | Equal to `value_type*`
`const_pointer` | Equal to `const value_type*`
`difference_type` | A type that can express the difference between two elements of type `iterator`. Usually `std::ptrdiff_t`
`size_type` | A type that can express the size of the range. Usually `size_t`


#### Member functions

Function | Description
---|---
`size_t size()` | Return the size of the mapped file
`iterator begin()` | Return an iterator to the beginning of the file
`iterator end()` | Return an iterator past the end of the file
`value_type operator[] (size_t i)` | Return the i'th character of the file
`bool empty()` | Returns true if the file is empty or no file is mapped
`void swap(file_map& other)` | Swap the file map with another
`file_map& operator=(file_map&& other)` | Move-assign another file map in place of this one


### Parsing

Parlay has some rudimentary support for converting to/from character sequences and primitive types. Currently, none of these methods perform any error handling, so their behavior is unspecified if attempting to convert between inappropriate types.

```c++
inline int chars_to_int(const parlay::sequence<char>& s)
```
```c++
inline long chars_to_long(const parlay::sequence<char>& s)
```
```c++
inline long long chars_to_long_long(const parlay::sequence<char>& s)
```
```c++
inline unsigned int chars_to_uint(const parlay::sequence<char>& s)
```
```c++
inline unsigned long chars_to_ulong(const parlay::sequence<char>& s)
```
```c++
inline unsigned long long chars_to_ulong_long(const parlay::sequence<char>& s)
```
```c++
inline float chars_to_float(const parlay::sequence<char>& s)
```
```c++
inline double chars_to_double(const parlay::sequence<char>& s)
```
```c++
inline long double chars_to_long_double(const parlay::sequence<char>& s)
```

**chars_to_int** attempts to interpret a signed integer value from the given character sequence. Similarly, **chars_to_uint** attempts to interpret an unsigned integer value, and **chars_to_double** attempts to interpret a `double`. The other listed methods do what you would expect given their name.



## Memory Allocator

<small>**Usage: `#include <parlay/alloc.h>`**</small>

Parlay's memory allocator can be used a C++ allocator for a container, for example, for `std::vector`, and for parlays own `parlay::sequence`.

```c++
// Create a sequence whose buffer will be allocated using Parlay's allocator
auto seq = parlay::sequence<int, parlay::allocator<int>>(100000, 0);
```

The allocator can also be use directly without an underlying container. For this, Parlay provides **type_allocator**, which allocates memory for a particular static type.

```c++
using long_allocator = parlay::type_allocator<long>;
long* x = long_allocator::alloc();
*x = 5;
long_allocator::free(x);
```
