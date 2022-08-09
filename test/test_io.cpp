#include "gtest/gtest.h"

#include <fstream>
#include <sstream>

#include <parlay/io.h>
#include <parlay/primitives.h>


TEST(TestIO, TestCharsFromFile) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words\nAnother line";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  auto s = parlay::chars_from_file(filename, false);
  
  // Space and control characters are inconsistent
  // across platforms, so we ignore them.
  auto contents2 = parlay::filter(contents, [](unsigned char c) { return std::isprint(c); });
  auto s2 = parlay::filter(s, [](unsigned char c) { return std::isprint(c); });

  ASSERT_EQ(contents2.size(), s2.size());
  for (size_t i = 0; i < contents2.size(); i++) {
      ASSERT_EQ(contents2[i], s2[i]);
  }
}

TEST(TestIO, TestCharsToFile) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words\nAnother line\n";
  
  parlay::chars_to_file(parlay::to_chars(contents), filename);
  
  std::ifstream in(filename);
  
  std::string file;
  
  for (std::string line; getline(in, line); ) {
    file += line;
    file += "\n";
  }
  
  ASSERT_EQ(contents, file);
}

TEST(TestIO, TestCharsToStream) {
  std::string contents = "Words, words, words";
  std::stringstream ss;
  parlay::chars_to_stream(parlay::to_chars(contents), ss);
  std::string result;
  getline(ss, result);
  ASSERT_EQ(result, contents);
}

TEST(TestIO, TestCharsToStreamOperator) {
  std::string contents = "Words, words, words";
  std::stringstream ss;
  ss << parlay::to_chars(contents);
  std::string result;
  getline(ss, result);
  ASSERT_EQ(result, contents);
}
