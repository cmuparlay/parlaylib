
# ParlayLib

ParlayLib is a C++ library for developing efficient parallel algorithms and software on shared-memory multicore machines. It provides additional tools and primitives that go beyond what is available in the C++ standard library, and simplifies the task of programming provably efficient and scalable parallel algorithms. It consists of a sequence data type (analogous to std::vector), many parallel routines and algorithms, a work-stealing scheduler to support nested parallelism, and a scalable memory allocator. It has been developed over a period of seven years and used in a variety of software including the [PBBS benchmark suite](http://www.cs.cmu.edu/~pbbs/benchmarks.html), the [Ligra](http://jshun.github.io/ligra/), [Julienne](https://dl.acm.org/doi/pdf/10.1145/3087556.3087580), and [Aspen](https://github.com/ldhulipala/aspen) graph processing frameworks, the [Graph Based Benchmark Suite](https://github.com/ParAlg/gbbs), and the [PAM](https://cmuparlay.github.io/PAMWeb/) library for parallel balanced binary search trees, and an implementation of the TPC-H benchmark suite.

Parlay is designed to be reasonably portable by being built upon mostly standards-compliant modern C++. It builds on [GCC](https://gcc.gnu.org/) and [Clang](https://clang.llvm.org/) on Linux, GCC and Apple Clang on OSX, and Microsoft Visual C++ ([MSVC](https://visualstudio.microsoft.com/vs/)) and [MinGW](http://www.mingw.org/) on Windows. It is also tested on GCC and Clang via Windows Subsystem for Linux ([WSL](https://docs.microsoft.com/en-us/windows/wsl/about)) and [Cygwin](https://www.cygwin.com/).

## Getting started

See the [installation](./installation.md) guide for instructions on installing Parlay and including it in your program. Parlay is a header-only library with no external dependencies, so once you've got it, its really easy to use. The most important components of Parlay to become familiarized with are the **parallel for loop**, the **sequence** container, and Parlay's library of parallel algorithms. As an example, here is an implementation of a [prime number seive](https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes) using Parlay.

```c++
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

parlay::sequence<long> prime_sieve(long n) {
  if (n < 2) return parlay::sequence<long>();
  else {
    long sqrt = std::sqrt(n);
    auto primes_sqrt = prime_sieve(sqrt);
    parlay::sequence<bool> flags(n+1, true);  // flags to mark the primes
    flags[0] = flags[1] = false;              // 0 and 1 are not prime
    parlay::parallel_for(0, primes_sqrt.size(), [&] (size_t i) {
      long prime = primes_sqrt[i];
      parlay::parallel_for(2, n/prime + 1, [&] (size_t j) {
        flags[prime * j] = false;
      });
    }, 1);
    return parlay::filter(parlay::iota<long>(n+1),
                          [&](size_t i) { return flags[i]; });
  }
}
```

This code demonstrates several of the key features mentioned above. The parallel for loop `parlay::parallel_for(i,j,f)` iterates over the range `[i,j)` and invokes the function `f` at each index. The `parlay::sequence` container holds an array of bools that is suitable for parallel computation (it is initialized and destructed in parallel). The `parlay::filter` algorithm takes a range and a predicate and returns a sequence consisting of copies of the elements of the range for which the predicate returns true. `iota(n)` returns a sequence consisting of the elements `0` to `n-1`.

## Documentation

Documentation for Parlay is an ongoing effort.

### Data types

Parlay's core data types make it easy to work in parallel over collections of items. The *sequence* data type is analagous to `std::vector` and should be your default container when using Parlay.

* [sequence](./datatypes/sequence.md) - A parallel contiguous sequence container
* [delayed_sequence](./datatypes/delayed_sequence.md) - A lazy sequence that generates its elements on demand from a function
* [slice](./datatypes/slice.md) - A non-owning view of a random-access iterator pair
* [hashtable](./datatypes/hashtable.md) - A deterministic phase-concurrent hashtable

### Algorithms

Parlay's algorithms are designed to provide near-state-of-the-art performance and scalability for working on large datasets. Parlay supports parallel versions of the majority of the C++ standard library, and much more.

* [Core algorithms](./algorithms/primitives.md) - Parlay's core primitives library
* [Delayed sequence algorithms](./algorithms/delayed.md) - A library for collection-oriented programming with delayed sequences
* [I/O, parsing, and formatting](./algorithms/io.md) - Algorithms for I/O and strings
* [Random number generation](./algorithms/random.md) - Random generators suitable for use in parallel algorithms

### Other tools

Parlay includes additional tools that make it easier to write parallel and concurrent code.

* [Parallel scheduler](./other/scheduler.md) - A fast work-stealing scheduler
* [Memory allocator](./other/allocator.md) - A scalable pool-based memory allocator


