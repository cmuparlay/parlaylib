
# Random Number Generation

Generating random numbers for use in parallel algorithms can be quite tricky. Simple utilities like
`rand()` and C++'s built-in generators are not thread safe. You could instead choose to instantiate
a thread-local random number generator for each thread, but this comes with a different problem. Due
to the nondeterminism in scheduling parallel computations, the results, even if the generators were
seeded deterministically, could vary from one run to the next.

To address this, Parlay comes with a simple solution for generating reproducable random numbers in
parallel. The key is the `parlay::random_generator` class, which can be supplied to any of C++'s
random number generators as the source of random bits.

For example, to generate uniform random integers, write

```c++
parlay::random_generator gen;
std::uniform_int_distribution<vertex> dis(0, n-1);
```

then generate random numbers by invoking `dis(gen)`. Note that each distribution
should only be used by one thread at a time (they are not thread safe).

The generator can be supplied with a non-default seed as an optional argument.

```c++
parlay::random_generator gen(42);
```

To generate random numbers in parallel, the generator supports an `operator[]`,
which creates another generator seeded from the current state and a given seed.
For example, to generate random numbers in parallel, you could write

```c++
parlay::random_generator gen;
std::uniform_int_distribution<vertex> dis(0, n-1);
auto result = parlay::tabulate(n, [&](size_t i) {
  auto r = gen[i];
  return dis(r);
});
 ```

The quality of randomness should be good enough for simple randomized algorithms,
but should not be relied upon for anything that requires high-quality randomness.

