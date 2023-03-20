# Parallel scheduler

<small>**Usage: `#include <parlay/parallel.h>`**</small>

Parlay offers an interface for fork-join parallelism in the form of a binary fork operation, a parallel-for loop, and a blocked-for loop.

- [Parallel interfaces](#parallel-interfaces)
  * [Binary forking (parallel_do)](#binary-forking--parallel-do-)
  * [Parallel-for loops (parallel_for)](#parallel-for-loops--parallel-for-)
  * [Blocked-for loops (blocked_for)](#blocked-for-loops--blocked-for-)
- [Advanced concerns](#advanced-concerns)
  * [Granularity](#granularity)
  * [Conservative scheduling](#conservative-scheduling)

## Parallel interfaces

### Binary forking (parallel_do)

```c++
template <typename Lf, typename Rf>
void parallel_do(Lf&& left, Rf&& right, bool conservative = false)
```

The primitive `parallel_do` takes two function objects `left` and `right` and evaluates them in parallel. `left` and `right` should be invocable with zero arguments. They may return values, but the returned values will be ignored.

For example, we can write a recursive sum function like so, where the left and right halves are evaluated in parallel.

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

### Parallel-for loops (parallel_for)

```c++
template <typename F>
void parallel_for(size_t start, size_t end, F&& f, long granularity = 0, bool conservative = false);
```

A parallel for loop takes a start and end value, and a function object `f`, and evaluates `f` at every value in `[start, end)`. `f` must be invocable given a single argument of type `size_t`. `f` may return a value, but the returned value will be ignored.

Optionally, a `granularity` parameter may be given. See [below](#granularity).



```c++
// Write the sequence 0,1,2,... into A
parlay::sequence<int> A(100000);
parlay::parallel_for(0, 100000, [&](size_t i) {
  A[i] = i;
});
```

### Blocked-for loops (blocked_for)

```c++
template<typename F>
void blocked_for(size_t start, size_t end, size_t block_size, F&& f, bool conservative = false)
```

Block-parallel loops are similar to a parallel-for loop with a fixed granularity, but instead of evaluating `f` for each index of the range in parallel, the range is split into contiguous blocks of a given size, and `f` is evaluated for each block evaluated in parallel. This can be handy if you have a very optimized sequential algorithm that you can apply to each block of elements, and might be more efficient than a naive parallel-for over each element.  Unlike granularity, which is only a hint to the scheduler, the block size is exact (except for the final block if the range size is not divisible by the block size).

The function object `f` passed to `blocked_for` takes three arguments: The number of the block, the start index of the block, and the end index (exclusive) of the block.

```c++
void copy_sequence(const parlay::sequence<int>& source, parlay::sequence<int>& dest) {
  // This *might* be more efficient than a parallel-for copying each element individually
  parlay::blocked_for(0, source.size(), 512, [&](size_t i, size_t start, size_t end) {
    std::copy(source.begin() + start, source.begin() + end, dest.begin() + start);
  });
}
```

## Advanced concerns

### Granularity

The granularity of a parallel-for loop is a hint to the scheduler that the given number of iterations of the loop should be ran sequentially. For example, a loop with 100K iterations and a granularity of 1000 could execute 100 parallel tasks, each of which was responsible for evaluating 1000 iterations sequentially.

Note that the granularity is only approximate and the range is not guaranteed to be divided exactly into equal-size chunks of the same granularity.  For instance, Parlay's scheduler will run chunks of size *at most* the granularity sequentially, but may also divide parts of the range into smaller chunks. Other schedulers (e.g., Cilk, or TBB) may have different behavior for the granularity, so it should only ever be treated as an optimization hint, and never relied upon for any correctness property.  If you require a range to be divided exactly into blocks of a fixed size, use [blocked_for](#blocked-for-loops--blocked-for-).

Setting a granularity too low can lead to slowdown of the code due to the inherent overhead of parallel scheduling, so it is important to set the granularity such that each sequential block of work does a sufficient amount of work to make this overhead negligible. Setting the granularity too high may lead to a lack of parallelism, which will also cause a slowdown.

When left to its default value (zero), Parlay's scheduler will estimate a good granularity for the loop itself. The scheduler-estimated value is usually quite good and close to optimal, so it is rarely necessarily to manually tune the granularity, unless your parallel-for loop does very irregular and unpredictable amounts of work. Keep in mind that the optimal granularity might depend on factors such as the hardware on which your code is ran, so hand tuning can only go so far and may even slow your code down on other environments/machines.

### Conservative scheduling

All of the functions in the parallelism API above take an optional parameter `conservative`, which defaults to `false`. Most of the time you can ignore this. When it matters is if your code takes locks during parallel calls. If your code takes locks, you should call these functions with `conservative` set to `true`, since this will prevent the scheduler from executing the parallel jobs in an order that might lead to deadlock. This comes at the expense of slightly worse performance, so you should only use this when necessary.

Note that locks might occasionally be taken implicitly, even if they are not obvious. For example, initializing a static function-scope variable implicitly takes a lock since their initialization is required to be thread-safe.  Therefore if your code might race to initialize a function-local static during several calls in parallel, you should set `conservative` to `true` for these calls.
