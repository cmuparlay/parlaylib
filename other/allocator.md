# Memory Allocator

<small>**Usage: `#include <parlay/alloc.h>`**</small>

Parlay's memory allocator can be used as a C++ allocator for a container, for example, for `std::vector`, and for parlays own `parlay::sequence`. Note that `parlay::allocator` is already the default allocator for `parlay::sequence`.

```c++
// Create a sequence whose buffer will be allocated using Parlay's allocator
auto seq = parlay::sequence<int, parlay::allocator<int>>(100000, 0);
```

The allocator can also be use directly without an underlying container. For this, Parlay provides **type_allocator**, which allocates memory for a particular static type. This is more efficient when allocating large numbers of objects of the same type, since their size is known at compile time and hence the allocator can hand out headerless blocks of memory.

```c++
using long_allocator = parlay::type_allocator<long>;
long* x = long_allocator::alloc();
*x = 5;
long_allocator::free(x);
```

