# Phase-concurrent Deterministic Hashtable

<small>**Usage: `#include <parlay/hash_table.h>`**</small>

```c++
template <
    typename Hash
> class hashtable
```

A phase-concurrent hashtable allows for concurrent insertions, concurrent searches, and concurrent deletions, but does not permit mixing insertions, searching, and deletion concurrently. The behavior of the table is specified via a customization point `Hash` policy. Hashing policies must provide a representation of an "empty" value. A default policy for hashing integer values is given by the `hash_numeric<T>` type, where `T` is an integer type. This policy uses `-1` as the empty value, and hence can not be used to store `-1` in the table.

```c++
parlay::hashtable<hash_numeric<int>> table;
table.insert(5);
auto val = table.find(5);  // Returns 5
table.deleteVal(5);
```

## Reference

- [Template parameters](#template-parameters)
- [Constructors](#constructors)
- [Member types](#member-types)
- [Member functions](#member-functions)
- [Non-member functions](#non-member-functions)


### Template parameters

* **Hash** is the customization point that specifies the hashing policy

### Constructors



### Member types



### Member functions



### Non-member functions


