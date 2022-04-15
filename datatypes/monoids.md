
# Associative Operations (Monoids)

Many of Parlay's algorithms, such as reduce and scan, can take an optional argument that specifies
how elements should be combined. This argument should be a binary [https://en.wikipedia.org/wiki/Associative_property](associative operation) $f$
with an identity element (an element `I` such that `f(x,I) = f(I,x) = x` for any `x`), sometimes called a [monoid](https://en.wikipedia.org/wiki/Monoid).

## Built-in associative operations

Parlay comes with a set of common associative operations with appropriate identities.

Operation | Default Identity | Description
---|---
`plus<T>` | `T{0}` | Adds two elements using `operator+`
`multiplies<T>` | `T{1}` | Multiplies two elements using `operator*`
`logical_and<T>` | `T{true}` | Multiplies two elements using `operator&&`
`logical_or<T>` | `T{false}` | Adds two element using `operator||`
`bit_or<T>` | `T{0}` | Adds two elements using `operator|`
`bit_xor<T>` | T{0}` | Adds two element using `operator^`
`bit_and<T>` | `~T{0}` | Multiplies two elements using `operator&`
`maximum<T>` | `numeric_limits<T>::lowest()` | Returns the greater of two elements using `operator>`
`minimum<T>` | `numeric_limits<T>::max()` | Returns the lesser of two elements using `operator<`

## User-defined operations

The easiest way to define your own custom associative operation with an identity is to use the
`binary_op` function, which takes two arguments: the associative function and the identity. For
example, suppose you had a `BasicMatrix<N>` class which represents an `NxN` matrix, and a function
`matrix_add`, which takes two matrices and produces their sum. This could be used with Parlay's
reduce and scan operations by creating a custom associative operation like so

```c++
template<typename T, size_t N>
struct BasicMatrix {
  T& operator()(size_t i, size_t j) { return m[i][j]; }
  const T& operator()(size_t i, size_t j) const { return m[i][j]; }

  BasicMatrix() : m(N, std::vector<T>(N)) {}
  static BasicMatrix zero() { return {}; }

 private:
  std::vector<std::vector<T>> m;
};

template<size_t N>
auto matrix_add(BasicMatrix<int, N> a, const BasicMatrix<int, N>& b) {
  for (size_t i = 0; i < N; i++) {
    for (size_t j = 0; j < N; j++) {
      a(i, j) += b(i, j);
    }
  }
  return a;
};

auto matrix3_op = parlay::binary_op(matrix_add<3>, BasicMatrix<int,3>::zero());
```
