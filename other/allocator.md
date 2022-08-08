# Memory Allocator

<small>**Usage: `#include <parlay/alloc.h>`**</small>

Scalable parallel algorithms require a scalable memory allocator in order to reach their full potential. Parlay's memory allocators are designed to provide allocation that scales to as many threads as you need.

**Using Parlay's allocator with a scalable malloc**: Note that Parlay's allocators are designed to be complementary to, and not a substitute for a scalable malloc implementation like [jemalloc](https://github.com/jemalloc/jemalloc) and [tcmalloc](https://github.com/google/tcmalloc). For the best possible performance, you should use **both** a scalable malloc implementation (jemalloc or tcmalloc or your favourite alternative) and Parlay's high-level allocators.

Parlay comes equipped with three high-level utilities for memory allocation:

* [parlay::p_malloc](#p_malloc): A general-purpose substitute for `malloc` / `operator new`
* [parlay::allocator\<T\>](#allocator): A container allocator for allocating arrays of type `T[]`
* [parlay::type_allocator\<T\>](#type_allocator): An allocator for allocating individual objects of type `T`

  
## p_malloc

```c++
void* p_malloc(size_t size);

void* p_malloc(size_t size, size_t align);
```
```c++
void p_free(void* ptr);
```

`p_malloc` provides a general-purpose substitute for C's `malloc` and C++'s `operator new`. It allocates `size` bytes of uninitialized storage, which, by default, is aligned as `alignof(std::max_align_t)` (8 or 16 bytes on most platforms). Optionally, a second argument `align` can be provided to request storage that is aligned to the given alignment. `p_free` is used to deallocate storage obtained by `p_malloc`.

## allocator

```c++
template<typename T>
struct allocator;
```

`parlay::allocator` provides a drop-in replacement for C++'s standard container allocator, `std::allocator`, and can therefore be used inside any C++ container that takes a [custom allocator](https://en.cppreference.com/w/cpp/named_req/Allocator). For example, you can use `std::vector` with Parlay's container allocator by writing the type `std::vector<int, parlay::allocator<int>>`. It is a stateless allocator, so all instances are interchangeable, compare equal and can deallocate memory allocated by any other instance of the same allocator type.

Note that `parlay::allocator` is the default allocator for `parlay::sequence`, so you do not have to specify it, unless the definition `PARLAY_USE_STD_ALLOC` is set, in which case, `std::allocator` will be used by default instead.

#### Template parameters

* **T** is the type of the elements of the container.

#### Constructors

```c++
constexpr allocator() noexcept;

template<typename U>
constexpr allocator(const allocator<U>&) noexcept;
```

Default construct an allocator or copy-construct an allocator from another allocator, possibly of a different type. `parlay::allocator` is stateless so all of its constructors are no-ops, except in the case of the very first allocator constructed during a program, which may trigger initialization of the underlying internal memory pools.

#### Member types

Type | Definition
---|---
`value_type` | Same as `T`


### Member functions

Function | Description
---|---
`T* allocate(size_t n)` | Allocate storage suitable for an array object of type `T[n]`
`void deallocate(T* ptr, size_t n)` | Deallocate storage obtained by a previous call to `allocate(n)`

## type_allocator

```c++
template<typename T>
struct type_allocator;
```

For allocating individual objects of a fixed type `T`, Parlay provides `type_allocator<T>`, which is designed to be much more efficient than using `p_malloc(sizeof(T), alignof(T))` or `parlay::allocator<T>{}.allocate(1)`. It is ideal for implementing pointer-based containers that have to allocate a large number of individual nodes. `type_allocator<T>` is static, which means you don't need to create an instance of it to use it.

For example, you might allocate nodes for a tree-based data structure like so:

```c++
struct Node {
  double value;
  std::atomic<Node*> left, right;
  Node(double value_) : value(value_), left(nullptr), right(nullptr) { }
};

using node_allocator = parlay::type_allocator<Node>;  // Convenient alias (optional)

Node* node = node_allocator::create(3.14);  // Constructs a node in the allocated storage by calling Node{3.14}
node_allocator::destroy(node);              // Destroys the node and deallocates its storage
```

#### Template parameters

* **T** is the type of the object to allocate storage for


### Static functions

`type_allocator` supports constructing and destructing an object directly via `create` and `destroy`.

Function | Description
---|---
`static T* create(Args... args)` | Allocate storage for and then construct an object of type `T` using `T(args...)`
`static void destroy(T* ptr)` | Destroy an object obtained by `create(...)` and deallocate its storage

It also supports allocating uninitialized storage suitable for an object of type `T` without constructing an object using `alloc` and `free`.

Function | Description
---|---
`static T* alloc()` | Allocate storage suitable for an object of type `T` but does not create any object in the storage
`static void free(T* ptr)` | Deallocate storage obtained by a previous call to `alloc()`
