#include "gtest/gtest.h"

#include <fstream>
#include <numeric>
#include <vector>

#include <parlay/io.h>

TEST(TestFileMap, TestConstruction) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
}

TEST(TestFileMap, TestReadContents) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
  
  ASSERT_TRUE(std::equal(std::begin(contents), std::end(contents), std::begin(f)));
}

TEST(TestFileMap, TestSubscript) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
  
  for (size_t i = 0; i < contents.size(); i++) {
    ASSERT_EQ(f[i], contents[i]);
  }
}

TEST(TestFileMap, TestMoveConstruct) {
  std::string filename = "test.txt";
  std::string contents = "Words, words, words";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
  
  parlay::file_map f2(std::move(f));
  ASSERT_TRUE(f.empty());
  ASSERT_FALSE(f2.empty());
  
  ASSERT_TRUE(std::equal(std::begin(contents), std::end(contents), std::begin(f2)));
}

TEST(TestFileMap, TestMoveAssign) {
  std::string filename = "test.txt";
  std::string filename2 = "test2.txt";
  std::string contents = "Words, words, words";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  std::ofstream out2(filename2);
  out2 << " " << std::endl;
  out2.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
  
  parlay::file_map f2(filename2);
  ASSERT_FALSE(f2.empty());
  
  f2 = std::move(f);
  ASSERT_TRUE(f.empty());
  ASSERT_FALSE(f2.empty());
  
  ASSERT_TRUE(std::equal(std::begin(contents), std::end(contents), std::begin(f2)));
}

TEST(TestFileMap, TestSwap) {
  std::string filename = "test.txt";
  std::string filename2 = "test2.txt";
  std::string contents = "Words, words, words";
  std::string contents2 = "Stuff, stuff, stuff";
  
  // Create a file first
  std::ofstream out(filename);
  out << contents << std::endl;
  out.close();
  
  std::ofstream out2(filename2);
  out2 << contents2 << std::endl;
  out2.close();
  
  parlay::file_map f(filename);
  ASSERT_FALSE(f.empty());
  
  parlay::file_map f2(filename2);
  ASSERT_FALSE(f2.empty());
  
  std::swap(f, f2);
  ASSERT_FALSE(f.empty());
  ASSERT_FALSE(f2.empty());
  
  ASSERT_TRUE(std::equal(std::begin(contents), std::end(contents), std::begin(f2)));
  ASSERT_TRUE(std::equal(std::begin(contents2), std::end(contents2), std::begin(f)));
}
