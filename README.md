# ParlayLib - A Toolkit for Programming Parallel Algorithms on Shared-Memory Multicore Machines

ParlayLib is a C++ library for developing efficient parallel algorithms and software on shared-memory multicore machines. It provides additional tools and primitives that go beyond what is available in the C++ standard library, and simplifies the task of programming provably efficient and scalable parallel algorithms. It consists of a sequence data type (analogous to std::vector), many parallel routines and algorithms, a work-stealing scheduler to support nested parallelism, and a scalable memory allocator. It has been developed over a period of seven years and used in a variety of software including the [PBBS benchmark suite](http://www.cs.cmu.edu/~pbbs/benchmarks.html), the [Ligra](http://jshun.github.io/ligra/), [Julienne](https://dl.acm.org/doi/pdf/10.1145/3087556.3087580), and [Aspen](https://github.com/ldhulipala/aspen) graph processing frameworks, the [Graph Based Benchmark Suite](https://github.com/ParAlg/gbbs), and the [PAM](https://cmuparlay.github.io/PAMWeb/) library for parallel balanced binary search trees, and an implementation of the TPC-H benchmark suite.

## Getting started

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

To ensure that Parlay functions correctly once included, make sure that you are compiling with C++17 enabled (add `-std=c++17` or later to your compile command, or the equivalent in your favourite build system). Some features of Parlay require a 16-byte CAS. This can be enabled by adding the definition `-DMCX16` to your compile command, and passing the flag `-mcx16` to supporting compilers. Lastly, you'll need threading support, which is usually achieved via one of the flags `-pthread`, `-pthreads`, or your operation system's equivalent.

### The old fashioned way

If fancy build systems are not your thing, the tried and true way to include Parlay in your project is to simply copy the source code of Parlay directly into your own project. Since the library is header only, this should work out of the box, assuming you add any required flags (see above). One possible way to do this while still enabling updates to ParlayLib is to include it as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) of your project's Git repository.

# Features

Parlay includes a number of pieces that can be used together or individually. At a high level, we have

* A library of useful parallel primitives
* Parallel data structures
* A parallel scheduler
* A scalable memory allocator

## Ranges

Many of Parlays primitive are designed around the *range* concept. Essentially, a range in Parlay is any type that supports `std::begin(r)` and `std::end(r)`, such that `std::begin(r)` returns a random access iterator. In other words, it is any type that can be used as a random access sequence. This is satisfied by `std::vector`, and by Parlays own `parlay::sequence`, and many other types.

## Parallel scheduler

Parlay offers an interface for fork-join parallelism in the form of a fork operation, and a parallel for loop. The primitive `par_do` takes two function objects and evaluates them in parallel. For exampe, we can write a recursive sum function like so, where the left and right halves are evaluated in parallel.

```c++
template<typename Iterator>
int sum(Iterator first, Iterator last) {
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

A parallel for loop takes a start and end value, and a function object, and evaluates the function object at every value in [start, end).

```c++
parlay::sequence<int> a(100000);
parlay::parallel_for(0, 100000, [&](size_t i) {
  a[i] = i;
});
```

## Data structures

### Sequence

A **sequence** is a parallel version of `std::vector`. It supports the same operations, but performs all initialization, updates, and destruction in parallel. Sequences satisfiy the range concept.

```c++
// A sequence consisting of 1000 copies of 5
auto seq = parlay::sequence<int>(1000, 5);
```

### Delayed Sequence

A delayed sequence is a lazy functional sequence that generates its elements on demand, rather than storing them in memory. A delayed sequence satisfies the range concept.

```c++
// A sequence consisting of 1000 copies of 5
auto seq = parlay::delayed_sequence<int>(1000, [](size_t i) {
  return 2*i + 1;
});
```

### Phase-concurrent Hashtable

A phase-concurrent hashtable allows for concurrent insertions, concurrent searches, and concurrent deletions, but does not permit mixing insertions, searching, and deletion concurrently.

```c++
parlay::hashtable<hash_numeric<int>> table;
table.insert(5);
auto val = table.find(5);  // Returns 5
table.deleteVal(5);
```

## Parallel algorithms

**tabulate** takes an integer n and and a function f of integers, and produces a sequence consisting of f(0), f(1), ..., f(n-1)

```c++
// Create a sequence consisting of the first 100 square numbers
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
```

**map** takes a range r and a function f from the value type of that range, and produces the sequence consisting of f(r[0]), f(r[1]), ...

```c++
auto seq = parlay::map(parlay::tabulate(100, [](int i) { return i*i; });,
  [](int x) { return 2 * x; }));
```

**dmap** (Delayed map) is the same as map, but the resulting sequence is a delayed sequence

```c++
auto seq = parlay::dmap(parlay::tabulate(100, [](int i) { return i*i; });,
  [](int x) { return 2 * x; }));
```

**copy** takes a given range and copies its elements into another range
```c++
// Copy the contents of seq1 into seq2
auto seq1 = parlay::tabulate(100, [](int i) { return i*i; });
auto seq2 = parlay::sequence<int>(100);
parlay::copy(seq1, seq2);
```

**reduce** takes a range and returns the reduction with respect some associative binary operation (addition by default). The associative operation is specified by a monoid object which is an object that has a `.identity` field, and a binary operator `f`.

```c++
// Default operation is sum
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
auto sum = parlay::reduce(seq1);

// With a custom monoid
struct {
  int identity = 0;
  int f(int x, int y) { return std::max(x, y ); }
} monoid;

auto max_val = parlay::reduce(seq, monoid);
```

**scan** computes a scan (aka prefix sum) with respect to an associative binary operation (addition by default).  The associative operation is specified by a monoid object which is an object that has a `.identity` field, and a binary operator `f`. Scan returns a pair, consisting of the partial sums, and the total.

```c++
// Default operation is sum
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
auto [prefix_sums, total] = parlay::scan(seq);
```

By default, scan considers prefix sums excluding the final element. There is also **scan_inclusive**, which is inclusive of the final element of each prefix. There are also inplace versions of each of these (**scan_inplace**, **scan_inclusive_inplace**), which write the sums into the input.

```c++
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
parlay::scan_inplace(seq);
```

**pack** takes an input a range a boolean indicator sequence, and returns a new sequence consisting of the elements of the range such that the element in the corresponding position in the indicator sequence is true.

```c++
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
auto indicator = parlay::delayed_sequence<bool>(100, 
  [](size_t i) { return i % 2 == 0; };

// Keep every second element
auto alternates = parlay::pack(seq, indicator));
```

Similarly, **pack_into** does the same thing but writes the answer into an existing range. **pack_index** takes a range of elements that are convertible to bool, and returns a sequence of indices such that the elements at those positions convert to true.

**filter** takes a range an a unary operator, and returns a sequence consisting of the elements of the range for which the unary operator returns true.

```c++
// Filter for the even numbers
auto seq = parlay::tabulate(100, [](int i) { return i*i; });
auto evens = parlay::filter(seq, [](int x) { return x % 2 == 0; });
```

Alternatively, **filter_into** does the same thing but writes the output into the given range and returns the number of elements that were kept.

**histogram** takes an integer valued range and a maximum value and returns a histogram, i.e. an array recording the number of occurrences of each element in the input range.

**sort** takes a given range and outputs a sorted copy (unlike the standard library, sort is not inplace by default). **sort_inplace** can be used to sort a given range in place. **stable_sort** and **stable_sort_inplace** are the same but guarantee that equal elements maintain their original relative order. All of these functions can optionally take a custom comparator object, which is a binary operator that evaluates to true if the first of the given elements should compare less than the second.

```c++
auto seq = parlay::tabulate(100, [](int i) { return (i*i) % 10; });
auto sorted = parlay::sort(seq);
```

**integer_sort** works just like sort, except that it is specialized to sort integer keys, and is significantly faster than ordinary sort. It can be used to sort sequences of integers, or sequences of arbitrary types provided that an integer key can be provided for each object.

```
// Sort a sequence of strings by their length using integer_sort
void sort_by_length(parlay::sequence<std::string>& s) {
  parlay::integer_sort(s, [](const auto& s) { return s.length(); });
}
```

## Memory Allocator

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

# Using Parlay with Cilk, OpenMP, or TBB

If you're already using Cilk, OpenMP, or Thread Building Blocks, and just want to use Parlay's algorithms without its parallel scheduler, that is easy to do. When building your program, simply add the appropriate compile definition as below.

```
-DPARLAY_CILK
-DPARLAY_OPENMP
-DPARLAY_TBB
```

Parlay will then use the specified framework's parallel operations to support its algorithms instead of its own scheduler.
