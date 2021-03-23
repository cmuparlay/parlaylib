#include "gtest/gtest.h"

#include <cmath>

#include <fstream>
#include <limits>
#include <sstream>

#include <parlay/io.h>

TEST(TestParsing, TestCharsToInt) {
  auto convert = [](int x) { return parlay::chars_to_int(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<int>::min)(), convert((std::numeric_limits<int>::min)()));
  ASSERT_EQ((std::numeric_limits<int>::max)(), convert((std::numeric_limits<int>::max)()));
  
  for (int x = (std::numeric_limits<int>::min)() / 2;
           x < (std::numeric_limits<int>::max)() / 2;
           x += (std::numeric_limits<int>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}

TEST(TestParsing, TestCharsToLong) {
  auto convert = [](long x) { return parlay::chars_to_long(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<long>::min)(), convert((std::numeric_limits<long>::min)()));
  ASSERT_EQ((std::numeric_limits<long>::max)(), convert((std::numeric_limits<long>::max)()));
  
  for (long x = (std::numeric_limits<long>::min)() / 2;
           x < (std::numeric_limits<long>::max)() / 2;
           x += (std::numeric_limits<long>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}


TEST(TestParsing, TestCharsToLongLong) {
  auto convert = [](long long x) { return parlay::chars_to_long_long(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<long long>::min)(), convert((std::numeric_limits<long long>::min)()));
  ASSERT_EQ((std::numeric_limits<long long>::max)(), convert((std::numeric_limits<long long>::max)()));
  
  for (long long x = (std::numeric_limits<long long>::min)() / 2;
           x < (std::numeric_limits<long long>::max)() / 2;
           x += (std::numeric_limits<long long>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}

TEST(TestParsing, TestCharsToUnsignedInt) {
  auto convert = [](unsigned int x) { return parlay::chars_to_uint(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<unsigned int>::min)(), convert((std::numeric_limits<unsigned int>::min)()));
  ASSERT_EQ((std::numeric_limits<unsigned int>::max)(), convert((std::numeric_limits<unsigned int>::max)()));
  
  for (unsigned int x = (std::numeric_limits<unsigned int>::min)() / 2;
           x < (std::numeric_limits<unsigned int>::max)() / 2;
           x += (std::numeric_limits<unsigned int>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}

TEST(TestParsing, TestCharsToUnsignedLong) {
  auto convert = [](unsigned long x) { return parlay::chars_to_ulong(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<unsigned long>::min)(), convert((std::numeric_limits<unsigned long>::min)()));
  ASSERT_EQ((std::numeric_limits<unsigned long>::max)(), convert((std::numeric_limits<unsigned long>::max)()));
  
  for (unsigned long x = (std::numeric_limits<unsigned long>::min)() / 2;
           x < (std::numeric_limits<unsigned long>::max)() / 2;
           x += (std::numeric_limits<unsigned long>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}


TEST(TestParsing, TestCharsToUnsignedLongLong) {
  auto convert = [](unsigned long long x) { return parlay::chars_to_ulong_long(parlay::to_chars(std::to_string(x))); };
  
  ASSERT_EQ(0, convert(0));
  ASSERT_EQ((std::numeric_limits<unsigned long long>::min)(), convert((std::numeric_limits<unsigned long long>::min)()));
  ASSERT_EQ((std::numeric_limits<unsigned long long>::max)(), convert((std::numeric_limits<unsigned long long>::max)()));
  
  for (unsigned long long x = (std::numeric_limits<unsigned long long>::min)() / 2;
           x < (std::numeric_limits<unsigned long long>::max)() / 2;
           x += (std::numeric_limits<unsigned long long>::max)() / 10000) {
    
    ASSERT_EQ(x, convert(x));
  }
}


TEST(TestParsing, TestCharsToFloatBig) {
  auto convert_std = [](float x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return std::stof(ss.str());
  };
  
  auto convert_parlay = [](float x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return parlay::chars_to_float(parlay::to_chars(ss.str()));
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0f), convert_parlay(0.0f));
  ASSERT_EQ(convert_std((std::numeric_limits<float>::min)()), convert_parlay((std::numeric_limits<float>::min)()));
  ASSERT_EQ(convert_std(-(std::numeric_limits<float>::min)()), convert_parlay(-(std::numeric_limits<float>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<float>::lowest()), convert_parlay(std::numeric_limits<float>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<float>::max)()), convert_parlay((std::numeric_limits<float>::max)()));

  if constexpr (std::numeric_limits<float>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<float>::infinity())));
  }
  if constexpr (std::numeric_limits<float>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<float>::quiet_NaN())));
  }
  
  // Test some random floats
  for (float x = std::numeric_limits<float>::lowest() / 2;
           x < (std::numeric_limits<float>::max)() / 2;
           x += (std::numeric_limits<float>::max)() / 10000) {
    
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }
  
  // Test some whole numbers
  for (int x = -2000000; x < 2000000; x += 419) {
    float y = static_cast<float>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}

TEST(TestParsing, TestCharsToDoubleBig) {
  auto convert_std = [](double x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return std::stod(ss.str());
  };
  
  auto convert_parlay = [](double x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return parlay::chars_to_double(parlay::to_chars(ss.str()));
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0), convert_parlay(0.0));
  ASSERT_EQ(convert_std((std::numeric_limits<double>::min)()), convert_parlay((std::numeric_limits<double>::min)()));
  ASSERT_EQ(-convert_std((std::numeric_limits<double>::min)()), convert_parlay(-(std::numeric_limits<double>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<double>::lowest()), convert_parlay(std::numeric_limits<double>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<double>::max)()), convert_parlay((std::numeric_limits<double>::max)()));

  if constexpr (std::numeric_limits<double>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<double>::infinity())));
  }
  if constexpr (std::numeric_limits<double>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<double>::quiet_NaN())));
  }
  
  // Test some random floats
  for (double x = std::numeric_limits<double>::lowest() / 2;
           x < (std::numeric_limits<double>::max)() / 2;
           x += (std::numeric_limits<double>::max)() / 10000) {
    
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }
  
  // Test some whole numbers
  for (long long x = -9000000000000000LL; x < 9000000000000000LL; x += 900719925474) {
    double y = static_cast<double>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}

TEST(TestParsing, TestCharsToLongDoubleBig) {
  auto convert_std = [](long double x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return std::stold(ss.str());
  };
  
  auto convert_parlay = [](long double x) {
    std::stringstream ss;
    ss << std::setprecision(20) << x;
    return parlay::chars_to_long_double(parlay::to_chars(ss.str()));
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0), convert_parlay(0.0));
  ASSERT_EQ(convert_std((std::numeric_limits<long double>::min)()), convert_parlay((std::numeric_limits<long double>::min)()));
  ASSERT_EQ(-convert_std((std::numeric_limits<long double>::min)()), convert_parlay(-(std::numeric_limits<long double>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<long double>::lowest()), convert_parlay(std::numeric_limits<long double>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<long double>::max)()), convert_parlay((std::numeric_limits<long double>::max)()));

  if constexpr (std::numeric_limits<long double>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<long double>::infinity())));
  }
  if constexpr (std::numeric_limits<long double>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<long double>::quiet_NaN())));
  }
  
  // Test some random floats
  for (long double x = std::numeric_limits<long double>::lowest() / 2;
           x < (std::numeric_limits<long double>::max)() / 2;
           x += (std::numeric_limits<long double>::max)() / 10000) {
    
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }
  
  // Test some whole numbers
  for (long long x = -9000000000000000LL; x < 9000000000000000LL; x += 900719925474) {
    long double y = static_cast<long double>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}

TEST(TestParsing, TestCharsToFloatSmall) {
  auto convert_std = [](float x) {
    std::stringstream ss;
    ss << std::setprecision(7) << x;
    return std::stof(ss.str());
  };
  
  auto convert_parlay = [](float x) {
    std::stringstream ss;
    ss << std::setprecision(7) << x;
    return parlay::chars_to_float(parlay::to_chars(ss.str()));
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0f), convert_parlay(0.0f));
  
  // 1.000001 * is required since converting to a string rounds towards zero, which will take min below the range of representable values!
  ASSERT_EQ(convert_std(1.000001f * (std::numeric_limits<float>::min)()), convert_parlay(1.000001f * (std::numeric_limits<float>::min)()));
  ASSERT_EQ(convert_std(-1.000001f * (std::numeric_limits<float>::min)()), convert_parlay(-1.000001f * (std::numeric_limits<float>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<float>::lowest()), convert_parlay(std::numeric_limits<float>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<float>::max)()), convert_parlay((std::numeric_limits<float>::max)()));

  if constexpr (std::numeric_limits<float>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<float>::infinity())));
  }
  if constexpr (std::numeric_limits<float>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<float>::quiet_NaN())));
  }
  
  // Test some random floats
  for (float x = -1.3e9; x < 1.3e9; x+= 3.1415e5) {
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }
  
  // Test some whole numbers
  for (int x = -2000000; x < 2000000; x += 31) {
    float y = static_cast<float>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}

TEST(TestParsing, TestCharsToDoubleSmall) {
  auto convert_std = [](double x) {
    std::stringstream ss;
    ss << std::setprecision(14) << x;
    auto res = std::stod(ss.str());
    return res;
  };
  
  auto convert_parlay = [](double x) {
    std::stringstream ss;
    ss << std::setprecision(14) << x;
    auto res = parlay::chars_to_double(parlay::to_chars(ss.str()));
    return res;
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0f), convert_parlay(0.0f));
  
  // 1.0000000000001 * is required since convering to a string rounds towards zero, which will take min below the range of representable values!
  ASSERT_EQ(convert_std(1.0000000000001 * (std::numeric_limits<double>::min)()), convert_parlay(1.0000000000001 * (std::numeric_limits<double>::min)()));  
  ASSERT_EQ(convert_std(-1.0000000000001 * (std::numeric_limits<double>::min)()), convert_parlay(-1.0000000000001 * (std::numeric_limits<double>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<double>::lowest()), convert_parlay(std::numeric_limits<double>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<double>::max)()), convert_parlay((std::numeric_limits<double>::max)()));

  if constexpr (std::numeric_limits<double>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<double>::infinity())));
  }
  if constexpr (std::numeric_limits<double>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<double>::quiet_NaN())));
  }

  // Test some random doubles
  for (double x = -1.3e21; x < 1.3e21; x+= 3.1415e16) {
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }

  // Test some whole numbers
  for (long long x = (std::numeric_limits<long long>::min)() / 2;
           x < (std::numeric_limits<long long>::max)() / 2;
           x += (std::numeric_limits<long long>::max)() / 100000) {
    double y = static_cast<double>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}

TEST(TestParsing, TestCharsToLongDoubleSmall) {
  auto convert_std = [](long double x) {
    std::stringstream ss;
    ss << std::setprecision(14) << x;
    auto res = std::stold(ss.str());
    return res;
  };
  
  auto convert_parlay = [](long double x) {
    std::stringstream ss;
    ss << std::setprecision(14) << x;
    auto res = parlay::chars_to_long_double(parlay::to_chars(ss.str()));
    return res;
  };

  // Test the corner cases
  ASSERT_EQ(convert_std(0.0f), convert_parlay(0.0f));
  
  // 1.0000000000001 * is required since convering to a string rounds towards zero, which will take min below the range of representable values!
  ASSERT_EQ(convert_std(1.0000000000001 * (std::numeric_limits<long double>::min)()), convert_parlay(1.0000000000001 * (std::numeric_limits<long double>::min)()));  
  ASSERT_EQ(convert_std(-1.0000000000001 * (std::numeric_limits<long double>::min)()), convert_parlay(-1.0000000000001 * (std::numeric_limits<long double>::min)()));
  ASSERT_EQ(convert_std(std::numeric_limits<long double>::lowest()), convert_parlay(std::numeric_limits<long double>::lowest()));
  ASSERT_EQ(convert_std((std::numeric_limits<long double>::max)()), convert_parlay((std::numeric_limits<long double>::max)()));

  if constexpr (std::numeric_limits<long double>::has_infinity) {
    ASSERT_TRUE(std::isinf(convert_parlay(std::numeric_limits<long double>::infinity())));
  }
  if constexpr (std::numeric_limits<long double>::has_quiet_NaN) {
    ASSERT_TRUE(std::isnan(convert_parlay(std::numeric_limits<long double>::quiet_NaN())));
  }

  // Test some random doubles
  for (long double x = -1.3e21L; x < 1.3e21L; x+= 3.1415e16L) {
    ASSERT_EQ(convert_std(x), convert_parlay(x));
  }
  
  // Test some whole numbers
  for (long long x = (std::numeric_limits<long long>::min)() / 2;
           x < (std::numeric_limits<long long>::max)() / 2;
           x += (std::numeric_limits<long long>::max)() / 100000) {
    long double y = static_cast<long double>(x);
    ASSERT_EQ(convert_std(y), convert_parlay(y));
  }
}
