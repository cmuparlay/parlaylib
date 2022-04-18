#include "gtest/gtest.h"

#include <limits>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

#include <parlay/io.h>

TEST(TestFormatting, TestCharToChars) {
  for (char c = '!'; c <= '~'; c++) {
    auto seq = parlay::to_chars(c);
    auto str = std::string(1, c);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  }
}

TEST(TestFormatting, TestBoolToChars) {
  for (int i = 0; i <= 1; i++) {
    bool b = static_cast<bool>(i);
    auto seq = parlay::to_chars(b);
    auto str = std::to_string(b);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  }
}

TEST(TestFormatting, TestLongToChars) {
  auto check = [](long x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<long>::min)());
  check((std::numeric_limits<long>::max)());

  for (long x = (std::numeric_limits<long>::min)() / 2;
       x < (std::numeric_limits<long>::max)() / 2;
       x += (std::numeric_limits<long>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestLongLongToChars) {
  auto check = [](long long x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<long long>::min)());
  check((std::numeric_limits<long long>::max)());

  for (long long x = (std::numeric_limits<long long>::min)() / 2;
       x < (std::numeric_limits<long long>::max)() / 2;
       x += (std::numeric_limits<long long>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestIntToChars) {
  auto check = [](int x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<int>::min)());
  check((std::numeric_limits<int>::max)());

  for (long x = (std::numeric_limits<int>::min)() / 2;
       x < (std::numeric_limits<int>::max)() / 2;
       x += (std::numeric_limits<int>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestULongToChars) {
  auto check = [](unsigned long x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<unsigned long>::min)());
  check((std::numeric_limits<unsigned long>::max)());

  for (unsigned long x = (std::numeric_limits<unsigned long>::min)() / 2;
       x < (std::numeric_limits<unsigned long>::max)() / 2;
       x += (std::numeric_limits<unsigned long>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestULongLongToChars) {
  auto check = [](unsigned long long x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<unsigned long long>::min)());
  check((std::numeric_limits<unsigned long long>::max)());

  for (unsigned long long x = (std::numeric_limits<unsigned long long>::min)() / 2;
       x < (std::numeric_limits<unsigned long long>::max)() / 2;
       x += (std::numeric_limits<unsigned long long>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestUIntToChars) {
  auto check = [](unsigned int x) {
    auto seq = parlay::to_chars(x);
    auto str = std::to_string(x);
    ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
  };

  check(0);
  check((std::numeric_limits<unsigned int>::min)());
  check((std::numeric_limits<unsigned int>::max)());

  for (unsigned int x = (std::numeric_limits<unsigned int>::min)() / 2;
       x < (std::numeric_limits<unsigned int>::max)() / 2;
       x += (std::numeric_limits<unsigned int>::max)() / 10000) { check(x); }
}

TEST(TestFormatting, TestDoubleToChars) {
  auto check = [](double x) {
    auto seq = parlay::to_chars(x);
    double back_to_double = std::stod(std::string(seq.begin(), seq.end()));
    double allowed_error = (std::max)(1e-9, std::abs(1e-9 * x));
    ASSERT_NEAR(x, back_to_double, allowed_error);
  };

  check(0.0);
  check((std::numeric_limits<double>::min)());
  check((std::numeric_limits<double>::max)());

  // Test some random big doubles
  for (double x = std::numeric_limits<double>::lowest() / 2;
       x < (std::numeric_limits<double>::max)() / 2;
       x += (std::numeric_limits<double>::max)() / 10000) { check(x); }

  // Test some random small doubles
  for (double x = -1.3e21L; x < 1.3e21L; x+= 3.1415e16L) {
    check(x);
  }

  // Test some whole numbers
  for (long long x = (std::numeric_limits<long long>::min)() / 2;
       x < (std::numeric_limits<long long>::max)() / 2;
       x += (std::numeric_limits<long long>::max)() / 100000) { check(static_cast<double>(x)); }
}

TEST(TestFormatting, TestFloatToChars) {
  auto check = [](float x) {
    auto seq = parlay::to_chars(x);
    float back_to_float = std::stof(std::string(seq.begin(), seq.end()));
    float allowed_error = (std::max)(1e-9f, std::abs(1e-9f * x));
    ASSERT_NEAR(x, back_to_float, allowed_error);
  };

  check(0.0);
  check((std::numeric_limits<float>::min)());
  check((std::numeric_limits<float>::max)());

  // Test some random big doubles
  for (float x = std::numeric_limits<float>::lowest() / 2;
       x < (std::numeric_limits<float>::max)() / 2;
       x += (std::numeric_limits<float>::max)() / 10000) { check(x); }

  // Test some random small doubles
  for (float x = -1.3e21f; x < 1.3e21f; x+= 3.1415e16f) {
    check(x);
  }

  // Test some whole numbers
  for (long long x = (std::numeric_limits<long long>::min)() / 2;
       x < (std::numeric_limits<long long>::max)() / 2;
       x += (std::numeric_limits<long long>::max)() / 100000) { check(static_cast<float>(x)); }
}

TEST(TestFormatting, TestStringToChars) {
  std::string str = "The small brown fox jumped over the lazy dog";
  auto seq = parlay::to_chars(str);
  ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str.data(), str.size()));
}

TEST(TestFormatting, TestCharArrayToChars) {
  const char* str = "The small brown fox jumped over the lazy dog";
  auto seq = parlay::to_chars(str);
  ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(str, std::strlen(str)));
}

TEST(TestFormatting, TestPairToChars) {
  auto p = std::make_pair<int, std::string>(5, "Hello, World");
  auto seq = parlay::to_chars(p);
  ASSERT_FALSE(seq.empty());
  std::string str(seq.begin(), seq.end());
  auto five_pos = str.find("5");
  auto str_pos = str.find("Hello, World", five_pos + 1);
  ASSERT_NE(five_pos, std::string::npos);
  ASSERT_NE(str_pos, std::string::npos);
  ASSERT_LT(five_pos, str_pos);
}

TEST(TestFormatting, TestSliceToChars) {
  std::vector<int> v(1000);
  std::iota(v.begin(), v.end(), 1);
  auto seq = parlay::to_chars(parlay::make_slice(v));
  ASSERT_FALSE(seq.empty());
  std::string str(seq.begin(), seq.end());
  auto last_pos = str.find("1");
  ASSERT_NE(last_pos, std::string::npos);
  for (size_t i = 2; i <= v.size(); i++) {
    auto pos = str.find(std::to_string(i), last_pos + 1);
    ASSERT_NE(pos, std::string::npos);
    ASSERT_LT(last_pos, pos);
    pos = last_pos;
  }
}

TEST(TestFormatting, TestSequenceNonChar) {
  auto v = parlay::sequence<int>(1000);
  std::iota(v.begin(), v.end(), 1);
  auto seq = parlay::to_chars(parlay::make_slice(v));
  ASSERT_FALSE(seq.empty());
  std::string str(seq.begin(), seq.end());
  auto last_pos = str.find("1");
  ASSERT_NE(last_pos, std::string::npos);
  for (size_t i = 2; i <= v.size(); i++) {
    auto pos = str.find(std::to_string(i), last_pos + 1);
    ASSERT_NE(pos, std::string::npos);
    ASSERT_LT(last_pos, pos);
    pos = last_pos;
  }
}

TEST(TestFormatting, TestString) {
  auto s = std::string("The small brown fox jumped over the lazy dog");
  auto seq = parlay::to_chars(s);
  ASSERT_EQ(std::string_view(seq.data(), seq.size()), std::string_view(s.data(), s.size()));

  // Check the rvalue overload
  auto seq2 = parlay::to_chars(s);
  ASSERT_EQ(std::string_view(seq2.data(), seq2.size()), std::string_view(s.data(), s.size()));
}

TEST(TestFormatting, TestRecursive) {
  auto p = std::make_pair(
            std::make_pair(
              1,
              std::make_pair(
                parlay::sequence<int>({1,2,3}),
                std::string("Hello"))),
              parlay::sequence<std::pair<int,int>>{{1,2}, {3,4}}
          );
  auto seq = parlay::to_chars(p);
  ASSERT_FALSE(seq.empty());
  std::string str(seq.begin(), seq.end());
  auto last_pos = str.find("1");
  ASSERT_NE(last_pos, std::string::npos);
  for (int i = 1; i <= 3; i++) {
    auto pos = str.find(std::to_string(i), last_pos + 1);
    ASSERT_NE(pos, std::string::npos);
    ASSERT_LT(last_pos, pos);
    last_pos = pos;
  }
  auto pos = str.find("Hello", last_pos + 1);
  ASSERT_NE(pos, std::string::npos);
  ASSERT_LT(last_pos, pos);
  last_pos = pos;
  for (int i = 1; i <= 4; i++) {
    pos = str.find(std::to_string(i), last_pos + 1);
    ASSERT_NE(pos, std::string::npos);
    ASSERT_LT(last_pos, pos);
    last_pos = pos;
  }
}
