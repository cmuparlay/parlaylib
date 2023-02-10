#ifndef PARLAY_IO_H_
#define PARLAY_IO_H_

#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <array>            // IWYU pragma: keep
#include <iterator>
#include <fstream>
#include <string>
#include <utility>

#include "primitives.h"
#include "sequence.h"
#include "slice.h"


// IWYU pragma: no_include <tuple>

namespace parlay {

// ----------------------------------------------------------------------------
//                                  I/O
// ----------------------------------------------------------------------------

// Reads a character sequence from a file between the bytes [start, end) :
//    if end is zero or larger than file, then returns full file
//    if start past end of file then returns an empty string.
// If null_terminate is true, a null terminator (an extra '0' character)
// is be appended to the sequence.
inline chars chars_from_file(const std::string& filename,
           bool null_terminate=false, std::streamoff start=0, std::streamoff end=0) {
  std::ifstream file (filename, std::ios::in | std::ios::binary | std::ios::ate);
  assert(file.is_open());
  auto length = static_cast<std::streamoff>(file.tellg());
  start = (std::min)(start,length);
  if (end == 0) end = length;
  else end = (std::min)(end,length);
  std::streamsize n = end - start;
  file.seekg (start, std::ios::beg);
  auto chars = chars::uninitialized(static_cast<size_t>(n) + null_terminate);
  file.read (chars.data(), n);
  file.close();
  if (null_terminate) {
    chars[static_cast<size_t>(n)] = 0;
  }
  return chars;
}

// Writes a character sequence to a stream
inline void chars_to_stream(const chars& S, std::ostream& os) {
  os.write(S.data(), static_cast<std::streamsize>(S.size()));
}

// Writes a character sequence to a file, returns 0 if successful
inline void chars_to_file(const chars& S, const std::string& filename) {
  std::ofstream file_stream (filename, std::ios::out | std::ios::binary);
  assert(file_stream.is_open());
  chars_to_stream(S, file_stream);
}

// Writes a character sequence to a stream
inline std::ostream& operator<<(std::ostream& os, const chars& s) {
  chars_to_stream(s, os);
  return os;
}

}

#include "internal/file_map.h"  // IWYU pragma: export

namespace parlay {

// ----------------------------------------------------------------------------
//                                  Parsing
// ----------------------------------------------------------------------------

namespace internal {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

// Interpret a character sequence as an integral type Integer_.
//
// Faster than using std::stoi(std::string(...)) by a factor of
// approximately 3-4 since it doesn't have to allocate a new string.
template<typename Integer_, typename It_>
Integer_ chars_to_int_t(slice<It_, It_> str) {
  static_assert(std::is_integral_v<Integer_>);

  size_t i = 0;

  if constexpr (std::is_signed_v<Integer_>) {
    // Compute the negative of str[i...], assuming that the sign has already
    // been read. Why negative? Because in two's complement, the smallest
    // possible value has greater magnitude by one than the largest possible
    // value. That is, INT_MAX = 2147483647 but INT_MIN = -2147483648, so if
    // we stored the result as a positive integer, we would overflow when reading
    // INT_MIN, which is technically undefined behavior!
    auto read_digits = [&]() {
      Integer_ r = 0;
      while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i])))
        r = r * 10 - (str[i++] - '0');
      return r;
    };
    if (str[i] == '-') {
      i++;
      return read_digits();
    } else {
      if (str[i] == '+') i++;
      return -read_digits();
    }
  }
  else {
    auto read_digits = [&]() {
      Integer_ r = 0;
      while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i])))
        r = r * 10 + (str[i++] - '0');
      return r;
    };
    if (str[i] == '-') {
      i++;
      return -read_digits();
    } else {
      if (str[i] == '+') i++;
      return read_digits();
    }
  }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Interpret a character sequence as a double. Performs
// no error checking, so the behaviour is unspecified
// if the given sequence is not a valid double.
//
// Significantly faster than std::stod if the number
// is not too big since we can just read the digits and
// divide or multiple exactly.  Otherwise we fall back
// to std::stod / std::stof / std::stold.
template<typename Float_, size_t max_len, int64_t max_exp, int64_t max_man, typename FallbackF>
Float_ chars_to_float_t(const chars& s, FallbackF&& fallback) {

  static const Float_ pow_ten[] = {
    Float_{1e0},  Float_{1e1},  Float_{1e2},  Float_{1e3},  Float_{1e4},
    Float_{1e5},  Float_{1e6},  Float_{1e7},  Float_{1e8},  Float_{1e9},
    Float_{1e10}, Float_{1e11}, Float_{1e12}, Float_{1e13}, Float_{1e14},
    Float_{1e15}, Float_{1e16}, Float_{1e17}, Float_{1e18}, Float_{1e19},
    Float_{1e20}, Float_{1e21}, Float_{1e22}
  };

  // Fast Path
  auto str = make_slice(s);
  auto sz = str.size();
  if (sz <= max_len) {
    size_t i = 0;
    uint64_t r = 0;
    int64_t exponent = 0;
    bool is_negative = false;
    
    while (std::isspace(static_cast<unsigned char>(str[i]))) {
      i++;
    }
    
    // Detect sign
    if (str[i] == '-') {
      is_negative = true;
      i++;
    }
    else if (str[i] == '+') {
      i++;
    }
    
    // Catches inf and nan
    if (str[i] == 'i' || str[i] == 'n') {
      return fallback(s);
    }
    
    // Read digits
    while (i < sz && std::isdigit(static_cast<unsigned char>(str[i]))) {
      r = r * 10 + (str[i++] - '0');
    }
    
    // Whole number. No decimal point and no exponent. Easy.
    if (i == sz) {
      if (r < (uint64_t{1} << max_man)) {
        Float_ res = static_cast<Float_>(r);
        if (is_negative) res = -res;
        return res;
      }
      else {
        return fallback(s);
      }
    }
    
    // Found the exponent. No decimal point
    if (str[i] == 'e' || str[i] == 'E') {
      exponent = static_cast<int64_t>(internal::chars_to_int_t<uint64_t>(str.cut(i+1, sz)));
      i = sz;
    }
    // Found the decimal point. Continue looking until we find an exponent or the end
    else {
      assert(str[i] == '.' || str[i] == ',');
      int64_t period = static_cast<int64_t>(i++);
      
      while (i < sz && std::isdigit(static_cast<unsigned char>(str[i]))) {
        r = r * 10 + (str[i++] - '0');
      }
      
      exponent = -(static_cast<int64_t>(i) - period - 1);
      
      if (i < sz && (str[i] == 'e' || str[i] == 'E')) {
        exponent += static_cast<int64_t>(internal::chars_to_int_t<uint64_t>(str.cut(i+1, sz)));
        i = sz;
      }
    }

    assert(i == sz);
    
    // We can represent this exactly!
    if (-max_exp <= exponent && exponent <= max_exp && r < (uint64_t{1} << max_man)) {
      Float_ result = static_cast<Float_>(r);
      Float_ tens = exponent > 0 ? pow_ten[exponent] : pow_ten[-exponent];
      if (exponent < 0) result = result / tens;
      else if (exponent > 0) result = result * tens;
      if (is_negative) result = -result;
      return result;
    }
  }
  
  // Slow path: Just fall back to std::stof/std::stod/std::stold
  return fallback(s);
}

}

inline int chars_to_int(const chars& s) { return internal::chars_to_int_t<int>(make_slice(s)); }
inline long chars_to_long(const chars& s) { return internal::chars_to_int_t<long>(make_slice(s)); }
inline long long chars_to_long_long(const chars& s) { return internal::chars_to_int_t<long long>(make_slice(s)); }

inline unsigned int chars_to_uint(const chars& s) { return internal::chars_to_int_t<unsigned int>(make_slice(s)); }
inline unsigned long chars_to_ulong(const chars& s) { return internal::chars_to_int_t<unsigned long>(make_slice(s)); }
inline unsigned long long chars_to_ulong_long(const chars& s) { return internal::chars_to_int_t<unsigned long long>(make_slice(s)); }


inline float chars_to_float(const chars& s) {
  return internal::chars_to_float_t<float, 10, 10, 24>(s, [](const auto& str) {
    return std::stof(std::string(std::begin(str), std::end(str))); });
}

inline double chars_to_double(const chars& s) {
  return internal::chars_to_float_t<double, 18, 22, 53>(s, [](const auto& str) {
    return std::stod(std::string(std::begin(str), std::end(str))); });
}

inline long double chars_to_long_double(const chars& s) {
  return internal::chars_to_float_t<long double, 18, 22, 53>(s, [](const auto& str) {
    return std::stold(std::string(std::begin(str), std::end(str))); });
}


// ----------------------------------------------------------------------------
//                                Formatting
// ----------------------------------------------------------------------------

// Still a work in progress. TODO: Improve these?

inline chars to_chars(char c) {
  return chars(1, c);
}

inline chars to_chars(bool v) {
  return to_chars(v ? '1' : '0');
}

inline chars to_chars(long v) {
  constexpr int max_len = 21;
  char s[max_len + 1];
  int l = std::snprintf(s, max_len, "%ld", v);
  return chars(s, s + (std::min)(max_len - 1, l));
}

inline chars to_chars(int v) {
  return to_chars((long) v);
};

inline chars to_chars(unsigned long v) {
  constexpr int max_len = 21;
  char s[max_len + 1];
  int l = std::snprintf(s, max_len, "%lu", v);
  return chars(s, s + (std::min)(max_len - 1, l));
}

inline chars to_chars(unsigned int v) {
  return to_chars((unsigned long) v);
};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif

inline chars to_chars(long long v) {
  constexpr int max_len = 21;
  char s[max_len + 1];
  int l = std::snprintf(s, max_len, "%" PRId64, v);
  return chars(s, s + (std::min)(max_len - 1, l));
}

inline chars to_chars(unsigned long long v) {
  constexpr int max_len = 21;
  char s[max_len + 1];
  int l = std::snprintf(s, max_len, "%" PRIu64, v);
  return chars(s, s + (std::min)(max_len - 1, l));
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

inline chars to_chars(double v) {
  constexpr int max_len = 21;
  char s[max_len + 1];
  int l = std::snprintf(s, max_len, "%.11le", v);
  return chars(s, s + (std::min)(max_len - 1, l));
}

inline chars to_chars(float v) {
  return to_chars((double) v);
};

inline chars to_chars(const std::string& s) {
  return chars::from_function(s.size(), [&](size_t i) -> char { return s[i]; });
}

inline chars to_chars(const char* s) {
  size_t l = std::strlen(s);
  return chars::from_function(l, [&](size_t i) -> char { return s[i]; });
}

template<typename A, typename B>
chars to_chars(const std::pair<A, B>& p) {
  sequence<chars> s = {
      to_chars('('), to_chars(p.first),
      to_chars(std::string(", ")),
      to_chars(p.second), to_chars(')')};
  return flatten(s);
}

template<typename A, long unsigned int N>
chars to_chars(const std::array<A, N>& P) {
  if (N == 0) return to_chars(std::string("[]"));
  auto separator = to_chars(std::string(", "));
  return flatten(tabulate(2 * N + 1, [&](size_t i) {
    if (i == 0) return to_chars('[');
    if (i == 2 * N) return to_chars(']');
    if (i & 1) return to_chars(P[i / 2]);
    return separator;
  }));
}

template<typename T>
chars to_chars(const slice<T, T>& A) {
  auto n = A.size();
  if (n == 0) return to_chars(std::string("[]"));
  auto separator = to_chars(std::string(", "));
  return flatten(tabulate(2 * n + 1, [&](size_t i) {
    if (i == 0) return to_chars('[');
    if (i == 2 * n) return to_chars(']');
    if (i & 1) return to_chars(A[i / 2]);
    return separator;
  }));
}

template<class T>
chars to_chars(const sequence<T>& A) {
  return to_chars(make_slice(A));
}

inline chars to_chars(const chars& s) {
  return s;
}

inline chars to_chars(chars&& s) {
  return std::move(s);
}

}  // namespace parlay

#endif  // PARLAY_IO_H_
