

# Core Algorithms

<small>**Usage: `#include <parlay/primitives.h>`**</small>

Parlay's core algorithms provide parallel versions of the majority of the C++ standard library, and more.

## Reference

- [Sequence production](#sequence-production)
  * [Tabulate & Delayed tabulate](#tabulate---delayed-tabulate)
  * [Iota](#iota)
- [Sequence copying/modification operations](#sequence-copying-modification-operations)
  * [Map & Delayed map](#map---delayed-map)
  * [Copy](#copy)
  * [Pack](#pack)
  * [Filter](#filter)
  * [Map maybe](#map-maybe)
  * [Unique](#unique)
  * [Remove duplicates](#remove-duplicates)
  * [Reverse](#reverse)
  * [Rotate](#rotate)
  * [Merge](#merge)
  * [Remove](#remove)
  * [Flatten](#flatten)
  * [Tokens](#tokens)
  * [Split](#split)
  * [Append](#append)
  * [Zip](#zip)
- [Sorting and grouping](#sorting-and-grouping)
  * [Sort](#sort)
  * [Integer Sort](#integer-sort)
  * [Group by](#group-by)
  * [Rank](#rank)
- [Reduction operations](#reduction-operations)
  * [Reduce](#reduce)
  * [Scan](#scan)
  * [Histogram](#histogram)
  * [Reduce by](#reduce-by)
- [Searching operations](#searching-operations)
  * [Count](#count)
  * [All of, any of, none of](#all-of--any-of--none-of)
  * [Find](#find)
  * [Adjacent find](#adjacent-find)
  * [Mismatch](#mismatch)
  * [Search](#search)
  * [Find end](#find-end)
  * [Min and max element](#min-and-max-element)
  * [k'th smallest](#k-th-smallest)
- [Sequence comparison](#sequence-comparison)
  * [Equal](#equal)
  * [Lexicographical compare](#lexicographical-compare)
  * [Is sorted](#is-sorted)
  * [Is partitioned](#is-partitioned)
- [Miscellaneous](#miscellaneous)
  * [For each](#for-each)


## Sequence production

### Tabulate & Delayed tabulate

```c++
template<typename UnaryOperator>
auto tabulate(size_t n, UnaryOperator&& f)

template<typename T, typename UnaryOperator>
auto tabulate(size_t n, UnaryOperator&& f)
```

```c++
template<typename UnaryOperator>
auto delayed_tabulate(size_t n, UnaryOperator f)

template<typename T, typename UnaryOperator>
auto delayed_tabulate(size_t n, UnaryOperator f)

template<typename T, typename V, typename UnaryOperator>
auto delayed_tabulate(size_t n, UnaryOperator f)
```

**tabulate** takes an integer `n` and and a function `f` on integers, and produces a `parlay::sequence` consisting of `f(0), f(1), ..., f(n-1)`. The first version of the function deduces the type of the sequence from the return type of `f` (formally, `std::decay_t<std::invoke_result_t<UnaryOperator, size_t>>`). The second version allows the type of the sequence to be explicitly specified.

**delayed_tabulate** does the same but returns a `parlay::delayed_sequence` instead. A delayed sequence generates its elements on demand rather than storing them in memory. Unlike regular sequences, delayed sequences may therefore produce prvalues (i.e. temporaries) as their reference type. Formally, for the first version, the reference type of the delayed sequence is deduced as `T = std::invoke_result_t<const F&, size_t>`, and the value type is deduced as `std::remove_cv_t<std::remove_reference_t<T>>`. The second version allows the reference type to specified explicitly as the type parameter `T`. The value type will be deduced as before. The third version allows both the reference type `T` and the value type `V` to specified explicitly.

Note that the function provided to delayed tabulate must be invokable as a const reference. That is, its `operator()` should be const qualified. This is true by default for lambdas. In other words, don't use `mutable` lambdas.

### Iota

```c++
template <typename Index>
auto iota(Index n)
```

**iota** returns a delayed sequence of the given template type consisting of the integers from `0` to `n-1`.

## Sequence copying/modification operations

### Map & Delayed map

```c++
template<typename Range, typename UnaryOperator>
auto map(Range&& r, UnaryOperator&& f)
```

```c++
template<typename Range, typename UnaryOperator>
auto delayed_map(Range&& r, UnaryOperator f)
```

**map** takes a random-access range `r` and a function `f` from the value type of that range, and produces the sequence consisting of `f(r[0]), f(r[1]), f(r[2]), ...`.

**delayed_map** is the same as map, but the resulting sequence is a delayed sequence. Note that **delayed_map** forwards the range argument to the closure owned by the delayed sequence, so if `r` is an rvalue reference, it will be moved into and owned by the delayed sequence. If it is an lvalue reference, the delayed sequence will not take ownership of it, so `r` must remain alive as long as the delayed sequence does. Delayed map internally keeps an iterator to the underlying range, so any operation that invalidates the iterators of `r` could invalidate the delayed map.



### Copy

```c++
template<typename RangeIn, typename RangeOut>
void copy(RangeIn&& in, RangeOut&& out)
```

**copy** takes a random-access range and copies its elements into another range by copy assignment.

### Pack

```c++
template<typename Range, typename BoolSeq>
auto pack(Range&& r, BoolSeq&& b)
```

```c++
template<typename BoolSeq>
auto pack_index(BoolSeq&& b) 

template<typename IndexType, typename BoolSeq>
auto pack_index(BoolSeq&& b) 
```

```c++
template<typename RangeIn, typename BoolSeq, typename RangeOut>
auto pack_into_uninitialized(RangeIn&& in, BoolSeq&& b, RangeOut&& out)
```

**pack** takes a random-access range `r` and a random-access boolean-valued indicator range, and returns a new sequence consisting of copies of the elements of `r` such that the element in the corresponding position in the indicator range is true.

**pack_index** takes a random-access range of elements that are convertible to bool, and returns a sequence of indices such that the elements at those positions convert to true. By default, pack index returns a sequence of `size_t`. The second version can explicitly specify the type of the indices.

**pack_into_uninitialized** does the same thing as pack, except it writes the answer into an existing range of uninitialized memory.

### Filter

```c++
template<typename Range, typename UnaryPredicate>
auto filter(Range&& r, UnaryPredicate&& f) 
```

```c++
template<typename RangeIn, typename RangeOut, typename UnaryPredicate>
auto filter_into_uninitialized(RangeIn&& in, RangeOut&& out, UnaryPredicate&& f)
```

**filter** takes a random-access range and a unary predicate, and returns a sequence consisting of copies of the elements of the range for which the predicate returns true. 

**filter_into_uninitialized** does the same thing but writes the output into the given range of uninitialized memory and returns the number of elements that were kept.

### Map maybe

```c++
template<typename Range, typename UnaryOperator>
auto map_maybe(Range&& v, UnaryOperator&& p)
```

**map_maybe** takes as input, a range and a function of the elements of that range that produces an `optional`. The result of `map_maybe` is a sequence consisting of the values that were present inside the non-empty optionals when the function is applied over the range. It is essentially a more efficient fusion of a map and a filter.

### Unique

```c++
template<typename Range>
auto unique(Range&& r)
```

```c++
template <typename Range, typename BinaryPredicate>
auto unique(Range&& r, BinaryPredicate eq)
```

**unique** takes a random-access range and returns a sequence consisting of copies of the elements of the given range that do not compare equal to the element preceding them. All elements in the output sequence maintain their original relative order. An optional binary predicate can be given to specify whether two elements should compare equal.

### Remove duplicates

```c++
template <typename Range>
auto remove_duplicates(Range&& A)

template <typename Range, typename Hash, typename Equal>
auto remove_duplicates(Range&& A, Hash&& hash, Equal&& equal)
```

```c++
template <typename Range>
auto remove_duplicates_ordered(Range&& r)

template <typename Range, typename BinaryPredicate>
auto remove_duplicates_ordered(Range&& r, BinaryPredicate&& less)
```

```c++
template <typename Integer_t, typename Range>
auto remove_duplicate_integers(Range&& A, Integer_t max_value)
```

**remove_duplicates** takes a random-access range `r` and produces a sequence consisting of copies of the elements of `r`, with exactly one occurrence of each element. The elements of `r` must be hashable and equality comparable. A custom hash function and equality predicate can be supplied to the second version. The hash function should return identical hashes for any pair of elements that the equality predicate deems equal. The order of the returned sequence is unspecified, as it depends on the hash function.

**remove_duplicates_ordered** is the same as remove duplicates, except that elements are compared using a less-than operator rather than hashing. The elements of `r` must be comparable, or a custom comparison predicate can be supplied that, given two elements, returns true if the first should compare less than the second. The resulting sequence is in sorted order.

**remove_duplicate_integers** is the same as remove duplicates, except that `r` must be a range of integer type of value at most `max_value`, and be convertible to `Integer_t`, an integer type. The result is a sequence of type `Integer_t` and is in sorted order.

### Reverse

```c++
template <typename Range>
auto reverse(Range&& r)
```

```c++
template <typename Range>
auto reverse_inplace(Range&& r)
```

**reverse** takes a random-access range `r` and returns a new sequence consisting of copies of the elements of `r` in reverse order. **reverse_inplace** reverses the order of the given range.

### Rotate

```c++
template <typename Range>
auto rotate(Range&& r, size_t t)
```

**rotate** takes a random-access range `r`  and returns a new sequence consisting of copies of the elements of the given range cyclically shifted left by `t` positions.

### Merge

```c++
template<typename Range1, typename Range2>
auto merge(Range1&& r1, Range2&& r2)
```

```c++
template<typename Range1, typename Range2, typename BinaryPredicate>
auto merge(Range1&& r1, Range2&& r2, BinaryPredicate&& pred)
```

**merge** returns a sequence consisting of copies of the elements of `r1` and `r2` in sorted order, assuming
that `r1` and `r2` are already sorted. `r1` and `r2` must have the same value type. An optional binary predicate can be used to specify the comparison operation. The binary predicate should take two values and return true if the first should compare less than the second.

### Remove

```c++
template<typename Range, typename T>
auto remove(Range&& r, const T& v)
```

```c++
template <typename Range, typename UnaryPredicate>
auto remove_if(Range&& r, UnaryPredicate&& pred)
```

**remove** takes a random-access range `r` and returns a sequence consisting of copies of the elements of the range `r` with any occurrences of `v` omitted. **remove_if** returns a sequence consisting of copies of the elements of the range `r` with any elements such that the given predicate returns true omitted.


### Flatten

```c++
template <typename Range>
auto flatten(Range&& r)
```

**flatten** takes a random-access range of random-access ranges and returns a single sequence consisting of copies of the elements of the underlying ranges in order.

### Tokens

```c++
template <typename Range>
sequence<sequence<char>> tokens(Range&& r)

template <typename Range, typename UnaryPredicate>
sequence<sequence<char>> tokens(Range&& r, UnaryPredicate&& is_space)
```

```c++
template <typename Range, typename UnaryOperator>
auto map_tokens(Range&& r, UnaryOperator f)

template <typename Range, typename UnaryOperator, typename UnaryPredicate>
auto map_tokens(Range&& r, UnaryOperator&& f, UnaryPredicate&& is_space)
```

**tokens** takes a random-access range of characters and returns a sequence consisting of the original characters split into "words", which are the maximal contiguous subsequences that do not contain any whitespace characters. Optionally, a custom criteria for determining the delimiters (whitespace by default) can be given. For example, to split a sequence at occurrences of commas, one could provide a value of `[](char c) { return c == ','; }` for `is_space`.

**map_tokens** splits the given character range into words in the same manner, but instead of returning a sequence of all the words, it applies the given unary operator `f` to every token. More specifically, `f` must be a function object that can take a `slice` of `value_type` equal to `char`. If `f` is a void function, i.e. returns nothing, then `map_tokens` returns nothing. Otherwise, if `f` returns values of type `T`, the result of `map_tokens` is a sequence of type `T` consisting of the results of evaluating `f` on each token. For example, to compute a sequence that contains the lengths of all of the words in a given input sequence, one could write

```c++
auto word_sizes = parlay::map_tokens(char_sequence, [](auto&& t) { return t.size(); });
```

In essence, `map_tokens` is just equivalent to `parlay::map(parlay::tokens(r), f)`, but is more efficient, because it avoids copying the tokens into new memory, and instead, applies the function `f` directly to the tokens in the original sequence.

### Split

```c++
template <typename Range, typename BoolRange>
auto split_at(Range&& r, BoolRange&& flags)
```

```c++
template <typename Range, typename BoolRange, typename UnaryOperator>
auto map_split_at(Range&& r, BoolRange&& flags, UnaryOperator&& f)
```

**split_at** takes a random-access ranges of characters and returns a sequence consisting of the original characters split into contiguous subsequences. Specifically, the subsequences are the maximal contiguous subsequences between positions such that the corresponding element in `flags` is true. This means that the number of subsequences is always one greater than the number of true flags. Also note that if there are adjacent true flags, the result can contain empty subsequences. The elements at positions corresponding to true flags are not themselves included in any subsequence.

**map_split_at** similarly splits the given sequence into contiguous subsequences, but instead of returning these subsequences, it applies the given unary operator `f` to each one. Specifically, `f` must be a function object that can take a `slice` of `value_type` equal to `char`. If `f` is a void function (i.e. returns nothing), then `map_split_at` returns nothing. Otherwise, `map_split_at` returns a sequence consisting of the results of applying `f` to each of the contiguous subsequences of `r`.

`map_split_at` is essentially equivalent to `parlay::map(parlay::split_at(r, flags), f)`, but is more efficient because the subsequences do not have to be copied into new memory, but are instead acted upon by `f` in place.

### Append

```c++
template <typename R1, typename R2>
auto append(R1&& s1, R2&& s2)
```

**append** takes two random-access ranges and produces a sequence consisting of the elements of the first range followed by the elements of the second range. The type of the resulting sequence is the value type of the first sequence.

### Zip

```c++
template<typename... Ranges>
auto zip(Ranges&&... rs)
```

**zip** takes a list of (at least two) random-access ranges `rs` and produces a sequence of tuples, where the i'th tuple contains the i'th element of each input range. The length of the zipped sequence is the length of the shortest range in `rs`.

## Sorting and grouping

### Sort

```c++
template<typename Range>
auto sort(Range&& in)

template<typename Range, typename BinaryPredicate>
auto sort(Range&& in, BinaryPredicate&& less)
```

```c++
template<typename Range>
auto stable_sort(Range&& in)

template<typename Range, typename BinaryPredicate>
auto stable_sort(Range&& in, BinaryPredicate&& less)
```

```c++
template<typename Range>
void sort_inplace(Range&& in)

template<typename Range, typename BinaryPredicate>
void sort_inplace(Range&& in, BinaryPredicate&& less)
```

```c++
template<typename Range>
void stable_sort_inplace(Range&& in)

template<typename Range, typename BinaryPredicate>
void stable_sort_inplace(Range&& in, BinaryPredicate&& less)
```


**sort** takes a given random-access range and outputs a sequence containing copies of the elements of the range in sorted order (note that unlike the standard library, sort is not inplace by default!). **sort_inplace** can be used to sort a given range in place. **stable_sort** and **stable_sort_inplace** are the same but guarantee that equal elements maintain their original relative order. All of these functions can optionally take a custom comparator object, which is a binary predicate that evaluates to true if the first of the given elements should compare less than the second.

### Integer Sort

```c++
template<typename Range>
auto integer_sort(Range&& in)

template<typename Range, typename Key>
auto integer_sort(Range&& in, Key&& key)
```

```c++
template<typename Range>
void integer_sort_inplace(Range&& in)

template<typename Range, typename Key>
void integer_sort_inplace(Range&& in, Key&& key)
```

```c++
template<typename Range, typename Key>
auto stable_integer_sort(Range&& in, Key&& key)
```

```c++
template<typename Range, typename Key>
void stable_integer_sort_inplace(Range&& in, Key&& key)
```

**integer_sort** works just like sort, except that it is specialized to sort integer keys, and is significantly faster than ordinary sort. It can be used to sort ranges of integers, or ranges of arbitrary types if a unary operator is provided that can produce an integer key for any given element. **stable_integer_sort** and **stable_integer_sort_inplace** are guaranteed to maintain the relative order between elements with equal keys.


### Group by

```c++
template <typename Range>
auto group_by_key(Range&& r)

template <typename Range, typename Hash>, typename Equal>
auto group_by_key(Range&& r, Hash&& hash, Equal&& equal)
```

```c++
template <typename Range>
auto group_by_key_ordered(Range&& s)

template <typename Range, typename BinaryPredicate>
auto group_by_key_ordered(Range&& s, BinaryPredicate&& less)
```

```c++
template <typename Integer_t, typename Range>
auto group_by_index(Range&& r, Integer_t num_buckets)
```

**group_by_key** takes a random-access range of key-value pairs `r` and produces a sequence of key-sequence pairs, where each key-sequence pair consists of a distinct key from `r`, and a sequence of copies of each value element of `r` with the corresponding key. The key elements of `r` must be hashable and equality comparable. The second version of the function allows a custom hash function and equality predicate to be supplied. The hash function should return identical hashes for any pair of elements that the equality predicate deems equal. The order of the returned sequence is unspecified, as it depends on the hash function.

**group_by_key_ordered** has the same behaviour as group by key, except that the elements of `r` are assumed to be less-than comparable, rather than hashable. The second version of the function takes a custom binary predicate that given two key elements from `r` should return true if the first compares less than the second. The order of the returned sequence is determined by the order of the keys.

**group_by_index** takes a random-access range of key-value pairs `r` and produces a sequence of sequences, where the i'th sequence in the result consists of copies of each value element of `r` with key i. The type of the keys must be integers, and a second parameter, `num_buckets` must be supplied, which indicates the value of the largest possible key.

### Rank

```c++
template<typename Range>
auto rank(Range&& r)

template<typename Range, typename BinaryPredicate>
auto rank(Range&& r, BinaryPredicate&& compare)

template<typename size_type, typename Range>
auto rank(Range&& r)

template<typename size_type, typename Range, typename BinaryPredicate>
auto rank(Range&& r, BinaryPredicate&& compare)
```

**rank** takes a random-access range and produces a sequence of indices denoting the rank of each corresponding item in the input range. The rank of an element is its position in the corresponding stably sorted list of the same elements. It optionally takes a custom comparator object, which is a binary predicate that evaluates to true if the first of the given elements should compare less than the second. Also optionally, the type of the resulting indices can be specified as a template parameter. By default, the type of the indices is `size_t`.

## Reduction operations

### Reduce

```c++
template<typename Range>
auto reduce(Range&& r)

template<typename Range, typename BinaryOperator>
auto reduce(Range&& r, BinaryOperator&& m)
```

**reduce** takes a random-access range and returns the reduction with respect some associative binary operation (addition by default). The associative operation is specified by a [binary operator](monoid.md) object. The type of the result is the value type of the range if no operator is supplied, otherwise it is the type returned by the binary operator.


### Scan

```c++
template<typename Range>
auto scan(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan(Range&& r, BinaryOperator&& m)
```

```c++
template<typename Range>
auto scan_inclusive(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan_inclusive(Range&& r, BinaryOperator&& m)
```

```c++
template<typename Range>
auto scan_inplace(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan_inplace(Range&& r, BinaryOperator&& m)
```

```c++
template<typename Range>
auto scan_inclusive_inplace(Range&& r)

template<typename Range, typename BinaryOperator>
auto scan_inclusive_inplace(Range&& r, BinaryOperator&& m)
```


**scan** takes a random-access range `r` and computes a scan (aka prefix sum) with respect to an associative binary operation (addition by default).  The associative operation is specified by a [binary operator](monoid.md) object. Scan returns a pair, consisting of a `parlay::sequence` containing the partial sums, and the total sum. The type of the resulting sums is the value type of `r` if no binary operator is specified, otherwise it is the type of the binary operator.

By default, scan considers prefix sums excluding the final element. There is also **scan_inclusive**, which is inclusive of the final element of each prefix. There are also inplace versions of each of these (**scan_inplace**, **scan_inclusive_inplace**), which write the sums into the input and return the total.


### Histogram


```c++
template<typename Range>
auto histogram_by_key(Range&& r)

template <typename sum_type, typename Range, typename Hash, typename Equal>
auto histogram_by_key(Range&& r, Hash&& hash, Equal&& equal)
```

```c++
template<typename Integer_, typename Range>
auto histogram_by_index(Range&& r, Integer_ m)
```

**histogram_by_key** takes a random-access range `A` and returns a sequence of key-value pairs, where the keys are the unique elements of `A`, and the values are the number of occurrences of the corresponding key in `A`. The key elements of `r` must be hashable and equality comparable. The second version of the function allows a custom hash function and equality predicate to be supplied. The hash function should return identical hashes for any pair of elements that the equality predicate deems equal. The order of the returned sequence is unspecified, as it depends on the hash function. An optional template argument, `sum_type` allows the type of the counter values to be customized. The default type is `size_t`. 

**histogram_by_index** takes an integer-valued random-access range `r` and a maximum value `m` and returns a sequence of length `m`, such that the i''th value of the sequence contains the number of occurrences of `i` in `r`. Every element in `r` must be at most `m`.





### Reduce by

```c++
template <typename Range>
auto reduce_by_key(Range&& r)

template <typename Range, typename BinaryOperator>
auto reduce_by_key(Range&& r, BinaryOperator&& op)

template <typename Range, typename BinaryOperator, typename Hash, typename Equal>
auto reduce_by_key(Range&& r, BinaryOperator&& op, Hash&& hash, Equal&& equal)
```

```c++
template <typename Range>
auto reduce_by_index(Range&& r, size_t num_buckets)

template <typename Range, typename BinaryOperator>
auto reduce_by_index(Range&& r, size_t num_buckets, BinaryOperator&& op)
```

**reduce_by_key** takes a random-access sequence `r` of key-value pairs and returns a random-access sequence of key-value pairs. The returned sequence consists of pairs with distinct keys from `r`, and values that are the sum of all of the value elements of `r` with the given key. By default, the sum is taken as addition, but the second version of the function can be supplied with a commutative [binary operator](monoid.md) object. The results are computed as type `T`, which defaults to `range_value_type_t<R>::second_type` for the first version, or the type of the binary operator for the second. The key type must be hashable and equality comparable. The third version of the function can be supplied a custom hash function and equality predicate. The hash function should return identical hashes for any pair of elements that the equality predicate deems equal. The order of the returned sequence is unspecified, as it depends on the hash function.

**reduce_by_index** similarly takes a random-access sequence `r` of key-value pairs and returns a sequence of values. The i'th element of the returned sequence consists of the sum of all of the value elements of `r` with key `i`. The type of the keys must be integers, and a second parameter, `num_buckets` must be supplied, which indicates the value of the largest possible key. As with reduce by key, an optional commutative [binary operator](monoid.md) may be supplied with which to perform the reduction (see `reduce_by_key`).

## Searching operations


### Count

```c++
template <typename Range, typename T>
size_t count(Range&& r, T const &value)
```

```c++
template <typename Range, typename UnaryPredicate>
size_t count_if(Range&& r, UnaryPredicate&& p)
```

**count** returns the number of elements in the given random-access range `r` that compare equal to the given value. **count_if** returns the number of elements for which the predicate `p` returns true.

### All of, any of, none of

```c++
template <typename Range, typename UnaryPredicate>
bool all_of(Range&& r, UnaryPredicate&& p)
```

```c++
template <typename Range, typename UnaryPredicate>
bool any_of(Range&& r, UnaryPredicate&& p)
```

```c++
template <typename Range, typename UnaryPredicate>
bool none_of(Range&& r, UnaryPredicate&& p)
```

**all_of**, **any_of**, and **none_of** return true if the predicate `p` is true for all, any, or none of the elements in the given random-access range `r` respectively.

### Find

```c++
template <typename Range, typename T>
auto find(Range&& r, T const &value)
```

```c++
template <typename Range, typename UnaryPredicate>
auto find_if(Range&& r, UnaryPredicate&& p)
```

```c++
template <typename Range, typename UnaryPredicate>
auto find_if_not(Range&& r, UnaryPredicate&& p)
```

```c++
template <typename Range1, typename Range2, typename BinaryPredicate>
auto find_first_of(Range1&& r1, Range2&& r2, BinaryPredicate&& p)
```

**find** returns an iterator to the first element in the given random-access range `r` that compares equal to the given value, or the end iterator if no such element exists. **find_if** returns the first element in the given range for which the predicate `p` is true, or the end iterator if no such element exists. **find_if_not** similarly returns the first element that does not satisfy the given predicate, or the end iterator.

**find_first_of** returns an iterator to the first element in the random-access range `r1` that compares equal to any of the elements in the random-access range `r2`, or the end iterator of `r1` if no such element exists.

### Adjacent find

```c++
template <typename Range>
auto adjacent_find(R&& r)

template <typename Range, typename BinaryPredicate>
auto adjacent_find(Range&& r, BinaryPredicate&& p)
```

**adjacent_find** returns an iterator to the first element in the given random-access range that compares equal to the next element on its right, Optionally, a binary predicate can be supplied to dictate how two elements should compare equal.

### Mismatch

```c++
template <typename Range1, typename Range2>
size_t mismatch(Range1&& r1, Range2&& r2)

template <typename Range1, typename Range2, typename BinaryPredicate>
auto mismatch(Range1&& r1, Range2&& r2, BinaryPredicate&& p)
```

**mismatch** returns a pair of iterators corresponding to the first occurrence in which an element of the random-access range `r1` is not equal to the element of the random-access range `r2` in the same position. If no such occurrence exists, returns a pair containing the end iterator of the shorter of the two ranges and an iterator pointing to the corresponding position in the other range. Optionally, a binary predicate can be supplied to specify how two elements should compare equal.

### Search

```c++
template <typename Range1, typename Range2>
size_t search(Range1&& r1, Range2&& r2)

template <typename Range1, typename Range2, typename BinaryPredicate>
auto search(Range1&& r1, Range2&& r2, BinaryPredicate&& pred)
```

**search** returns an iterator to the beginning of the first occurrence of the random-access range `r2` in the random-access range `r1`, or the end iterator of `r1` if no such occurrence exists. Optionally, a binary predicate can be given to specify how two elements should compare equal.

### Find end

```c++
template <typename Range1, typename Range2>
auto find_end(Range1&& r1, Range2&& r2)

template <typename Range1, typename Range2, typename BinaryPredicate>
auto find_end(Range1&& r1, Range2&& r2, BinaryPredicate&& p)
```

**find_end** returns an iterator to the beginning of the last occurrence of the random-access range `r2` in the random-access range `r1`, or the end iterator of `r1` if no such occurrence exists. Optionally, a binary predicate can be given to specify how two elements should compare equal.

### Min and max element

```c++
template <typename Range>
auto min_element(Range&& r)

template <typename Range, typename BinaryPredicate>
auto min_element(Range&& r, BinaryPredicate&& less)
```

```c++
template <typename Range>
auto max_element(Range&& r)

template <typename Range, typename BinaryPredicate>
auto max_element(Range&& r, BinaryPredicate&& less)
```

```c++
template <typename Range>
auto minmax_element(Range&& r)

template <typename Range, typename BinaryPredicate>
auto minmax_element(Range&& r, BinaryPredicate&& less)
```

**min_element** and **max_element** return an iterator to the minimum or maximum element in the given random-access range respectively. In the case of duplicates, the leftmost element is always selected. **minmax_element** returns a pair consisting of iterators to both the minimum and maximum element. An optional binary predicate can be supplied to specify how two elements should compare.

### k'th smallest

```c++
template <typename Range>
auto kth_smallest(Range&& in, size_t k)

template <typename Range, typename BinaryPredicate>
auto kth_smallest(Range&& in, size_t k, BinaryPredicate&& less)
```

```c++
template <typename Range>
auto kth_smallest_copy(Range&& in, size_t k)

template <typename Range, typename BinaryPredicate>
auto kth_smallest_copy(Range&& in, size_t k, BinaryPredicate&& less)
```

**kth_smallest** takes a random-access range and a position `k` and returns an iterator to the k'th smallest element in the range. It optionally takes a custom comparator object, which is a binary predicate that evaluates to true if the first of the given elements should compare less than the second.

**kth_smallest_copy** similarly takes a random-access range and a position `k` and returns a copy of the k'th smallest element in the range. Unlike `kth_smallest`, the elements of the range must therefore be copy-constructible. `kth_smallest_copy` is considerably more efficient than `kth_smallest` when the element type is cheap to copy. It optionally takes a custom comparator object, which is a binary predicate that evaluates to true if the first of the given elements should compare less than the second.

## Sequence comparison

### Equal

```c++
template <typename Range1, typename Range2>
bool equal(Range1&& r1, Range2&& r2)

template <typename Range1, typename Range2, typename BinaryPredicate>
bool equal(Range1&& r1, Range2&& r2, BinaryPredicate&& p)
```

**equal** returns true if the given ranges are equal, that is, they have the same size and all elements at corresponding positions compare equal. Optionally, a binary predicate can be given to specify whether two elements should compare equal.

### Lexicographical compare

```c++
template <typename Range1, typename Range2>
bool lexicographical_compare(Range1&& r1, Range2&& r2)

template <typename Range1, typename Range2, typename BinaryPredicate>
bool lexicographical_compare(Range1&& r1, Range2&& r2, BinaryPredicate&& less)
```

**lexicographical_compare** returns true if the first random-access range compares lexicographically less than the second random-access range. A range is considered lexicographically less than another if it is a prefix of the other or the first mismatched element compares less than the corresponding element in the other range. An optional binary predicate can be supplied to specify how two elements should compare.

### Is sorted

```c++
template <typename Range>
bool is_sorted(Range&& r)

template <typename Range, typename BinaryPredicate>
bool is_sorted(Range&& r, BinaryPredicate&& less)
```

```c++
template <typename Range>
auto is_sorted_until(Range&& r)

template <typename Range, typename BinaryPredicate>
auto is_sorted_until(Range&& r, BinaryPredicate&& less)
```

**is_sorted** returns true if the given random-access range is sorted. **is_sorted_until** returns an iterator to the first element of the range that is out of order, or the end iterator if the range is sorted. An optional binary predicate can be supplied to specify how two elements should compare.

### Is partitioned

```c++
template <typename Range, typename UnaryPredicate>
bool is_partitioned(Range&& r, UnaryPredicate&& f)
```

**is_partitioned** returns true if the given random-access range is partitioned with respect to the given unary predicate. A range is partitioned with respect to the given predicate if all elements for which the predicate returns true precede all of those for which it returns false.

## Miscellaneous

### For each

```c++
template <typename Range, typename UnaryFunction>
void for_each(Range&& r , UnaryFunction&& f)
```

**for_each** applies the given unary function to every element of the given random-access range. The range may be constant, in which case the unary function should not attempt to modify it, or it may be mutable, in which case the function is allowed to modify it.