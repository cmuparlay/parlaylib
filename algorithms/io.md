# I/O, Parsing, and Formatting

<small>**Usage: `#include <parlay/io.h>`**</small>

Parlay includes some convenient tools for file input and output in terms of `parlay::sequence<char>`, as well as tools for converting to and from `parlay::sequence<char>` and primitive types.

## Reading and writing files

```c++
inline parlay::sequence<char> chars_from_file(const std::string& filename,
    bool null_terminate, size_t start=0, size_t end=0)
```
```c++
inline void chars_to_file(const parlay::sequence<char>& S,
    const std::string& filename)
```

**chars_from_file** reads the contents of a local file into a character sequence. If `null_terminate` is true, the sequence will be ended by a null terminator (`\0`) character. This is only required for compatability with C APIs and otherwise isn't necessary. To read a particular portion of a file rather than its entirety, the parameters `start` and `end` can specify the positions of the first and last character to read. If `start` or `end` is zero, the file is read from to beginning and/or to the end respectively.

**chars_to_file** writes the given character sequence to the local file with the given name. If a file with the given name already exists, it will be overwritten.

## Writing character sequences to streams

```c++
inline void chars_to_stream(const sequence<char>& S, std::ostream& os)
```
```c++
inline std::ostream& operator<<(std::ostream& os, const sequence<char>& s)
```

Character sequences can also be written to standard streams, i.e. types deriving from `std::ostream`. They support the standard `operator<<`, as well as a method **chars_to_stream**, which takes a character sequence and a stream, and writes the given characters to the stream.

## Parsing

Parlay has some rudimentary support for converting to/from character sequences and primitive types. Currently, none of these methods perform any error handling, so their behavior is unspecified if attempting to convert between inappropriate types.

```c++
inline int chars_to_int(const parlay::sequence<char>& s)
```
```c++
inline long chars_to_long(const parlay::sequence<char>& s)
```
```c++
inline long long chars_to_long_long(const parlay::sequence<char>& s)
```
```c++
inline unsigned int chars_to_uint(const parlay::sequence<char>& s)
```
```c++
inline unsigned long chars_to_ulong(const parlay::sequence<char>& s)
```
```c++
inline unsigned long long chars_to_ulong_long(const parlay::sequence<char>& s)
```
```c++
inline float chars_to_float(const parlay::sequence<char>& s)
```
```c++
inline double chars_to_double(const parlay::sequence<char>& s)
```
```c++
inline long double chars_to_long_double(const parlay::sequence<char>& s)
```

**chars_to_int** attempts to interpret a signed integer value from the given character sequence. Similarly, **chars_to_uint** attempts to interpret an unsigned integer value, and **chars_to_double** attempts to interpret a `double`. The other listed methods do what you would expect given their name.


## Memory-mapped files

```c++
class file_map
```

To support reading large files quickly and in parallel, possibly even files that do not fit in main memory, Parlay provides a wrapper type **file_map** for memory mapping. Memory mapping takes a given file on disk, and maps its contents to a range of addresses in virtual memory. Memory mapped files are much more efficient than sequential file reading because their contents can be read in parallel. The `file_map` type is a random-access range, and hence can be used in conjunction with all of Parlay's parallel algorithms.

### Constructors

```c++
explicit file_map(const std::string& filename)
```

Memory maps the file with the given filename.

```c++
file_map(file_map&& other)
```

Objects of type `file_map` can be moved but not copied.


### Member types

Type | Definition
---|---
`value_type` | The value type of the underlying file contents. Usually `char`
`reference` | Equal to `value_type&`
`const_reference` | Equal to `const value_type&`
`iterator` | An iterator to a range of elements of type `value_type`
`const_iterator` | Equal to `const iterator`
`pointer` | Equal to `value_type*`
`const_pointer` | Equal to `const value_type*`
`difference_type` | A type that can express the difference between two elements of type `iterator`. Usually `std::ptrdiff_t`
`size_type` | A type that can express the size of the range. Usually `size_t`


### Member functions

Function | Description
---|---
`size_t size()` | Return the size of the mapped file
`iterator begin()` | Return an iterator to the beginning of the file
`iterator end()` | Return an iterator past the end of the file
`value_type operator[] (size_t i)` | Return the i'th character of the file
`bool empty()` | Returns true if the file is empty or no file is mapped
`void swap(file_map& other)` | Swap the file map with another
`file_map& operator=(file_map&& other)` | Move-assign another file map in place of this one


