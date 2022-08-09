
# parlay::delayed

<small>**Usage: `#include <parlay/delayed.h>`**</small>


**parlay::delayed** is a library for collection-oriented programming with delayed sequences. Collection-oriented programming is a paradigm of working with a sequence of data and applying composable operations such as map, reduce, filter, and scan to obtain some desired output. Delayed sequences are sequences that generate their elements on demand rather than storing them in memory. Their use in conjunction with collection-oriented programming can save significant amounts of memory and substantially improve the performance of memory-bound algorithms. A full list of algorithms in the library can be found [below](#reference).

The `parlay::delayed` library is based on our research group's recent work on *parallel block-delayed sequences* (Westrick, Rainey, Anderson, and Belloch; PPoPP '22), and inspired by the recent [C++ ranges library](https://en.cppreference.com/w/cpp/ranges).

- [Examples](#examples)
    - [Example: A prime sieve](#example-a-prime-sieve-that-uses-flatten)
    - [Example: Addition with carry propagation](#example-addition-with-carry-propagation)
    - [Example: Breadth-first search](#example-breadth-first-search)
- [Reference](#reference)
    - [Range operations](#range-operations)
      - [Map](#map)
      - [Scan](#scan)
      - [Flatten](#flatten)
      - [Filter](#filter)
      - [Map maybe / Filter op](#map-maybe--filter-op)
      - [Zip](#zip)
      - [Zip with](#zip-with)
      - [Enumerate](#enumerate)
    - [Terminal operations](#terminal-operations)
      - [To sequence](#to-sequence)
      - [Reduce](#reduce)
      - [For each / Apply](#for-each--apply)

## Examples

### Example: A prime sieve

For example, consider the problem of determining all of the prime numbers up to some integer n. One suboptimal way to implement this is like so.

```c++
parlay::sequence<long> primes(long n) {
  if (n < 2) return parlay::sequence<long>();
  long sqrt_n = std::sqrt(n); 
  auto sqrt_primes = primes(sqrt_n);
  parlay::sequence flags(n+1, true);
  auto sieves = parlay::map(sqrt_primes, [&] (long p) {
      return parlay::tabulate(n/p - 1, [&] (long m) {
          return (m+2)*p;
      });
  });
  auto s = parlay::flatten(sieves);
  parlay::parallel_for(0, s.size(), [&] (size_t i) {
      flags[s[i]] = false;
  });
  flags[0] = flags[1] = false;
  return parlay::filter(parlay::iota(n+1), [&](size_t i) { return flags[i]; });
}
```

This program makes use of the `flatten` primitive which takes a sequence of sequences and flattens it down into a single sequence. Although this program is highly parallel, it could be a lot more efficient, because it allocates quite a lot of memory. The `seives` variable stores a sequence of sequences of integers, then `s` stores a flattened copy of that sequence. To improve this, we can make use of delayed sequences. The first optimization is simply to use a delayed sequence in place of the inner sequence of `seive`, replacing `parlay::tabulate` on Line 7 with `parlay::delayed_tabulate`. This saves us from storing the intermediate sequences. Of course, the `s` sequence still exists and makes a copy of everything in `sieves`, so there is more memory to be saved.

To save more memory, we can replace `parlay::flatten` with `parlay::delayed::flatten`. Instead of generating the flattened sequence eagerly, `delayed::flatten` creates the flattened sequence on demand as it is iterated. Because of this, the result of `delayed::flatten` (and indeed most of the algorithms in the delayed library) is not a random-access sequence. It is a special kind of sequence called a *block-iterable* sequence, which is the underlying representation of most of the results of most algorithms in parlay::delayed. In order to make use of such a sequence, we can either use it as the input into a another delayed algorithm, or we can use a *terminal* algorithm, which takes a block-iterable sequence and returns a result that we can use. One such algorithm is `parlay::delayed::for_each` (a.k.a. `parlay::delayed::apply`), which takes a block-iterable sequence, invokes a given function on each element in parallel, and returns nothing. Applying this to the above code, we arrive at

```c++
parlay::sequence<long> primes(long n) {
  if (n < 2) return parlay::sequence<long>();
  long sqrt_n = std::sqrt(n);
  auto sqrt_primes = primes(sqrt_n);
  parlay::sequence flags(n+1, true);
  auto sieves = parlay::map(sqrt_primes, [&] (long p) {
      return parlay::delayed_tabulate(n/p - 1, [=] (long m) {
          return (m+2)*p;
      });
  });
  auto s = parlay::block_delayed::flatten(sieves);
  parlay::block_delayed::apply(s, [&] (long j) { flags[j] = false; });
  flags[0] = flags[1] = false;
  return parlay::filter(parlay::iota(n+1), [&](size_t i) { return flags[i]; });
}
```

Since `delayed::flatten` iterates over the flattened sequence on demand rather than creating and storing it explicitly, this version of the program stores no intermediate sequences. The only sequences stored are the recursive `sqprimes` and `flags`, which is used to obtain the result. The resulting code is therefore **2.2x faster** on 72 cores than the original code that stored all of the intermediate results. This version of the code is even 13% faster than the double nested for-loop example for prime sieving depicted in the getting started guide.

### Example: Addition with carry propagation

Consider the following algorithm for adding two larger integers, represented as sequences of bytes (unsigned char) in base 128.

```c++
using digit = unsigned char;
using bignum = parlay::sequence<digit>;
constexpr unsigned char BASE = 128;

auto big_add(const bignum& A, const bignum & B) {
  size_t n = A.size();
  auto sums = parlay::tabulate(n, [&] (size_t i) -> digit {
    return A[i] + B[i];  });
  auto f = [] (digit a, digit b) -> digit { // carry propagate
    return (b == BASE-1) ? a : b;  };
  auto [carries, total] = parlay::scan(sums, parlay::make_monoid(f, digit(BASE-1)));

  auto r = parlay::tabulate(n, [&, &carries = carries] (size_t i) -> digit {
    return ((carries[i] >= BASE) + sums[i]) % BASE;  });
  return r;
}
```

This algorithm makes use of the `parlay::scan` primitive for computing prefix sums. Note that like the previous example, it stores a lot of intermediate results which waste memory. Firstly to speed it up, we can store the `sums` sequence as delayed sequence by replacing `parlay::tabulate` with `parlay::delayed_tabulate`. Then, we can also turn the result of the scan into a delayed sequence by replacing `parlay::scan` with `parlay::delayed::scan`. Since delayed scan returns a block-iterable sequence, we can not perform the final tabulate on it since it requires random-access into the sequence. Since the final tabulate loops over corresponding elements of `carries` and `sums`,  we can use the `zip_with` primitive, which, given a collection of ranges, produces a single range whose elements are computed based on the corresponding element from each input range. The final result is then obtained by converting the output to an ordinary sequence via the terminal operation `to_sequence`. The final resulting code looks like this.

```c++
auto big_add(const bignum& A, const bignum & B) {
  size_t n = A.size();
  auto sums = parlay::delayed_tabulate(n, [&] (size_t i) -> digit {
    return A[i] + B[i]; });
  auto f = [] (digit a, digit b) -> digit { // carry propagate
    return (b == BASE-1) ? a : b;};
  auto [carries, total] = parlay::delayed::scan(sums, f, digit(BASE-1));

  auto mr = parlay::delayed::zip_with([](digit carry, digit sum) -> digit {
    return ((carry >= BASE) + sum) % BASE;
  }, carries, sums);
  auto r = parlay::delayed::to_sequence(mr);
  return r;
}
```

Since all of the intermediate operations are delayed, this program uses substantially less memory than the original one, and this results in a **3.1x speedup** on 72 cores.

### Example: Breadth-first search

Consider the parallel implementation of breadth-first search given below. It maintains the current frontier of the search in a sequence `frontier`, and each iteration, in parallel, computes the outgoing edges of the frontier and computes the set of vertices adjacent to the frontier that have not already been visited. Since the same vertex could be visited by multiple neighbours in the same round, a `compare_exchange` operation is used to set the parent of a vertex, and ensure that only the winning copy ends up in the next round.

```c++
using vertexId = int;
using adjList = parlay::sequence<parlay::sequence<vertexId>>;

parlay::sequence<vertexId> BFS(const adjList& G, vertexId start) {
  size_t n = G.size();
  auto parent = parlay::tabulate<std::atomic<vertexId>>(n, [&](size_t) { return -1; });
  parent[start] = start;
  parlay::sequence<vertexId> frontier(1,start);

  while (frontier.size() > 0) {

    // get out edges of the frontier and flatten
    auto nested_edges = parlay::map(frontier, [&] (vertexId u) {
      return parlay::tabulate(G[u].size(), [&, u] (size_t i) {
        return std::make_pair(u, G[u][i]);});});
    auto edges = parlay::flatten(nested_edges);

    // keep the v from (u,v) edges that succeed in setting the parent array at v to u
    auto edge_f = [&] (auto&& u_v) {
      vertexId expected = -1;
      auto [u, v] = u_v;
      return (parent[v] == -1) && parent[v].compare_exchange_strong(expected, u);
    };
    
    frontier = parlay::map(parlay::filter(edges, edge_f), [](auto&& x) { return x.second; });
  }

  // convert from atomic to regular sequence
  return parlay::map(parent, [] (auto&& x) { return x.load(); });
}
```

The most interesting operations are the use of `flatten` and `filter`. The `parlay::flatten` operation is used to combine the sequence of outgoing edges for each vertex in the frontier into a single sequence of outgoing edges. The `parlay::filter` operation is used to ensure that only only copy of each new vertex is added to the new frontier. In this example, both of these operations generate an intermediate sequence that is potentially wasteful of memory. To improve performance, we can delay the `edges` sequence by both converting the `parlay::map` into `parlay::delayed::map`, `parlay::tabulate` to a `parlay::delayed_tabulate` and then using `parlay::delayed::flatten` in place of `parlay::flatten`.

The next thing that be improved with delaying is the filtering of the next frontier. We could replace `parlay::map(parlay::filter(...` with their delayed equivalents, however, it turns out that there is a more efficient primitive for this combination of operations (a filter followed immediately by a map). The algorithm `parlay::delayed::map_maybe` takes as input, a range and a function of the elements of that range that produces an `optional`. The result of `map_maybe` is a range consisting of the values that were present inside the non-empty optionals when the function is applied over the range. This primitive is substantially more efficient than a manual combination of map and filter. Lastly, since `frontier` must be a `parlay::sequence` for the next iteration, we use `parlay::delayed::to_sequence` to convert the result of the sequence of delayed operations into a regular (non-delayed) sequence. The brings us to the following implementation.

```c++
parlay::sequence<vertexId> BFS(const adjList& G, vertexId start) {
  size_t n = G.size();
  auto parent = parlay::tabulate<std::atomic<vertexId>>(n, [&](size_t) { return -1; });
  parent[start] = start;
  parlay::sequence<vertexId> frontier(1,start);

  while (frontier.size() > 0) {

    // get out edges of the frontier and flatten
    auto nested_edges = parlay::delayed::map(frontier, [&] (vertexId u) {
      return parlay::delayed_tabulate(G[u].size(), [&, u] (size_t i) {
        return std::make_pair(u, G[u][i]);});});
    auto edges = parlay::delayed::flatten(nested_edges);

    // keep the v from (u,v) edges that succeed in setting the parent array at v to u
    auto edge_f = [&] (auto u_v) -> std::optional<vertexId> {
      vertexId expected = -1;
      auto [u, v] = u_v;
      if ((parent[v] == -1) && parent[v].compare_exchange_strong(expected, u))
        return std::make_optional(v);
      else return std::nullopt;
    };

    frontier = parlay::delayed::to_sequence(parlay::delayed::map_maybe(edges, edge_f));
  }

  // convert from atomic to regular sequence
  return parlay::map(parent, [] (auto&& x) { return x.load();});
}
```

In this case, the resulting delayed algorithm is **2.1x faster** than the original non-delayed code.

## Reference

### Range operations

Delayed range operations produce a range of values as their output. For some operations and input combinations, the result may be a random-access range, but it is most often a *block-iterable range*. A block-iterable range can be used as input to another delayed range operation, or as input to a terminal operation, which will produce a non-delayed-range output.

Throughout the following descriptions, we will refer frequently to a range's *value type* and its *reference type*. Informally, the reference type of a range is the type you get when you dereference one of its iterators. Confusingly, this need not be a reference-qualified type, because a delayed range could generate prvalues (temporaries)! The value type of a range is less well defined, but more or less means a type that can be used to store a non-dangling copy of an element of the range. Often, this is just the reference type with its cv and reference qualifiers removed, but need not be in all cases (see zip as an interesting example). Formally, these types are defined by the following traits:

Type | Definition
---|---
`range_iterator_type_t<R>` | `decltype(std::begin(std::declval<R&>()))`
`range_value_type_t<R>` | `std::iterator_traits<range_iterator_type_t<R>>::value_type`
`range_reference_type_t<R>` | `std::iterator_traits<range_iterator_type_t<R>>::reference`

**Lifetime and iterator validity:** Note, very importantly, that delayed range operations can take either an lvalue-reference to a range, or a moved or temporary range. In the first case, the delayed range will keep a reference to the underlying range, so the caller must ensure that the underlying range outlives the delayed range. Additionally, delayed ranges often store iterators to their underlying range, so it is required that once a delayed range is constructed with respect to an lvalue to an underlying range, no operations that could invalidate its iterators may be performed. If the underlying range is moved into or constructed from a temporary, then it is stored as a member of the delayed range, so its lifetime is tied to the delayed range and need not be managed by the caller. However, issues of iterator invalidation still apply. In particular, if the underlying range's iterators are invalidated by moving it, then it can not be stored as a value and must be stored as a reference, since otherwise, its iterators could be invalidated after the delayed range is constructed but before it is moved to its final destination.

All of the following operations are defined in the namespace `parlay::delayed`.

#### Map

```c++
template<typename Range, typename UnaryOperator>
auto map(Range&& r, UnaryOperator f)
```

The **map** operation takes as input a range `r` and a unary function `f` on the reference type of the range, and produces a delayed range consisting of the values obtained by applying `f` to each element of `r`. If `r` is a random-access range, then the result is also a random-access range. Otherwise, `r` must be a block-iterable range and the result will be a block-iterable range.

The reference type of the resulting range is the type returned by `f` on elements of `r`, or, formally, `std::invoke_result_t<UnaryOperator&, range_reference_type_t<Range>>`. The value type of the range is the same as the reference type but without cv or reference qualifiers. Note, perhaps usefully, that `f` need not return a type by value, but may return a reference-qualified type. In this case, the reference type of the resulting range will be reference qualified. If the return type of `f` is a prvalue (a.k.a. a temporary), the reference type of the resulting range is a prvalue.

#### Scan

```c++
template<typename Range>
auto scan(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan(Range&& r, BinaryOperator f)

template<typename T, typename Range, typename BinaryOperator>
auto scan(Range&& r, BinaryOperator f, T identity)
```

```c++
template<typename Range>
auto scan_inclusive(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan_inclusive(Range&& r, BinaryOperator f)

template<typename T, typename Range, typename BinaryOperator>
auto scan_inclusive(Range&& r, BinaryOperator f, T identity)
```

The **scan** operation takes as input a range `r`, a binary operator `f`, and an identity value `identity`, and produces the prefix sum of `r` with respect to the associative operator `f` with identity `identity`. `scan` returns a pair containing a range consisting of the (non-inclusive) prefix sums, and the total sum. `scan_inclusive` just returns a range consisting of the (inclusive) prefix sums. If not supplied, the binary operator defaults to addition (`std::plus`). If not supplied, the identity defaults to the default value of type `T`, which by default, is the value type of `r`. The input must be a random-access range or a block-iterable range, and the output is always a block-iterable range.

The reference and value type of the output range is `T`, so you should ensure that `T` is large enough to hold all possible results. For example, if you supply `identity = 0`, you are setting `T` to be `int`, regardless of the value type of `r`!

#### Flatten

```c++
template<typename Range>
auto flatten(Range&& r) {
```

The **flatten** operation takes as input a range of ranges `r` and produces a range consisting of the elements of the inner ranges appended together. `r` may be either a random-access range or a block-iterable range, and the result is always a block-iterable range. The inner ranges must be at least a forward range.

The reference and value types of the resulting range are those of the inner ranges. For instance, if the inner ranges do not produce prvalues, then it is possible to modify the original range via the flattened range, since the flattened range refers directly to the elements of the original ranges. If the reference type of `r` is a prvalue, then the outer ranges are materialized and moved into the result object to avoid the risk of dangling. The reference and value types of the resulting range are then those of the (now materialized) inner ranges.

#### Filter

```c++
template<typename Range, typename UnaryPredicate>
auto filter(Range&& r, UnaryPredicate p)
```

The **filter** operation takes as input a range `r` and a unary predicate `p` on the reference type of `r`, and produces a range consisting of the elements of `r` for which `p` returns true. `r` may be either a random-access range or a block-iterable range, and the result is always a block-iterable range.

The reference and value types of the resulting range are the same as those of `r`. If the reference type of `r` is a reference, then it is possible to modify the elements of `r` via the filtered range. Since filter preserves the value category of the underlying range, it may be more efficient to use `map_maybe` / `filter_op` if the values are cheap-to-copy primitives (e.g. ints) and you don't need to reference the original range. Consider using `may_maybe` / `filter_op` for better performance if this applies.

#### Map maybe / Filter op

```c++
template<typename Range, typename UnaryOperator>
auto map_maybe(Range&& r, UnaryOperator f)

template<typename Range, typename UnaryOperator>
auto filter_op(Range&& r, UnaryOperator f)  // Alias of map_maybe
```

The **map maybe** operation (a.k.a. fiter op) takes as input a range `r` and a unary function `f` on the reference type of `r` that returns instances of `std::optional`. It produces a range consisting of the values of the non-empty optionals obtained when applying `f` to the elements of `r`. It is in some sense therefore a more efficient implementation of a filter followed by a map. `r` may be either a random-access range or a block-iterable range, and the result is always a block-iterable range.

The value type of the range is the value type of the optional returned by `f` on the elements of `r`, or, formally, `std::invoke_result_t<UnaryOperator, range_reference_type_t<Range>>::value_type`. Map maybe precomputes the results of the optionals, so the reference type of the range is actually `value_type&`, which means that the caller may opt to move the value from the resulting range. The resulting values are only computed once, so if they are moved from, a subsequent iteration of the range will see moved-from values.

#### Zip

```c++
template<typename... Ranges>
auto zip(Ranges&&... rs)
```

The **zip** operation takes a list of (at least two) ranges `rs` and produces a range of tuples, where the i'th tuple contains the i'th element of each input range. The length of the zipped range is the length of the shortest range in `rs`. If all of the ranges `rs` are random-access ranges, then the result is a random-access range. Otherwise, all of the ranges in `rs` must be random-access ranges or block-iterable ranges, and the result is a block-iterable range.

The reference type of the zipped range is `std::tuple<range_reference_type_t<Ranges>...>`, and its value type is `std::tuple<range_value_type_t<Ranges>...>`. If the reference type of any of the underlying ranges is a reference, then it is possible to modify that original range via the zipped range.

#### Zip with

```c++
template<typename NaryOperator, typename... Ranges>
auto zip_with(NaryOperator f, Ranges&&... rs)
```

The **zip with** operation takes an N-ary function `f` and a list of N ranges `rs`, and produces a range where the i'th value is the result of applying `f` to the i'th element of each input range. `zip_with` is equivalent to a zip followed by a map, where the map function calls `std::apply(f, t)` for each tuple `t` in the zipped range. If all of the input ranges are random-access ranges, then the result is a random-access range. Otherwise all of the input ranges must be random-access ranges or block-iterable ranges, and the result is a block-iterable range.

The reference type of the resulting range is the type returned by `f` on the elements of `rs`, or, formally, `std::invoke_result_t<NaryOperator&, range_reference_type_t<Ranges>...>`. The value type of the range is the same as the reference type but without cv or reference qualifiers.

#### Enumerate

```c++
template<typename Range>
auto enumerate(Range&& r)
```

**Enumerate** takes a range and produces a range of pairs, such that the i'th pair consists of the integer i and the i'th element of `r`. It is equivalent to `zip(parlay::iota(parlay::size(r)), r)`.

### Terminal operations

Terminal operations take as input, a random-access delayed range or a block-iterable delayed range, and produce an output that is not a block-iterable range. These operations are also defined in the namespace `parlay::delayed`.

#### To sequence

```c++
template<typename Range>
auto to_sequence(Range&& r)

template<typename T, typename Alloc = /* default allocator */, typename Range>
auto to_sequence(Range&& r)
```

Given a range `r`, **to sequence** produces a `parlay::sequence` consisting of copies of the elements of `r`. By default, the value type of the sequence is the value type of `r`. To specify a different value type, the second overload can be used, which takes the desired value type, and optionally a custom allocator, to use for the sequence. The elements of the sequence will be explicitly initialized from the elements of the range, invoking any implicit or explicit conversion.

#### Reduce

```c++
template<typename Range>
auto reduce(Range&& r)

template<typename Range, typename BinaryOperator>
auto reduce(Range&& r, BinaryOperator&& f)

template<typename Range, typename BinaryOperator, typename T>
auto reduce(Range&& r, BinaryOperator&& f, T identity)
```

**Reduce** takes a range `r` and produces the sum of its elements with respect to a given binary operator `f` and identity value. By default, the binary operator is `std::plus<>`, and the initial value is `0` in the value type of the range. The result type is `T`, the same as the identity element. Note that this means that if you pass a literal `0` as identity, the result type will be `int`, regardless of the value type of `r`.

#### For each / Apply

```c++
template<typename Range,  typename UnaryFunction>
void for_each(Range&& r, UnaryFunction&& f)

template<typename Range, typename UnaryFunction>
void apply(Range&& r, UnaryFunction&& f)  // Alias of for_each
```

The **for each** operation (a.ka. apply) takes a range `r` and a unary function `f` on the reference type of `r`, and applies `f` to each element of `r` in parallel. The return value of `f`, if any, is ignored.


