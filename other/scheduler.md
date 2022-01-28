# Parallel scheduler

<small>**Usage: `#include <parlay/parallel.h>`**</small>

Parlay offers an interface for fork-join parallelism in the form of a fork operation, and a parallel for loop.

### Binary forking

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

### Parallel for loops

A parallel for loop takes a start and end value, and a function object, and evaluates the function object at every value in [start, end).

```c++
parlay::sequence<int> a(100000);
parlay::parallel_for(0, 100000, [&](size_t i) {
  a[i] = i;
});
```
