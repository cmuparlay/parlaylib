#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "suffix_tree.h"

// **************************************************************
// Driver code
// **************************************************************
using charseq = parlay::sequence<char>;
using uint = unsigned int;

auto check(parlay::sequence<char>& str) {
  auto ustr = parlay::map(str, [] (char x) {return (unsigned char) x;});
  auto less = [&] (uint i, uint j) {
    return parlay::lexicographical_compare(ustr.cut(i,ustr.size()),ustr.cut(j,ustr.size()));};
  return parlay::sort(parlay::iota<uint>(ustr.size()), less);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: suffix_tree <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    charseq str = parlay::chars_from_file(argv[1]);
    using index = unsigned int;
    long n = str.size();
    suffix_tree<uint> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      t.start();
      result = suffix_tree<uint>(str);
      t.next("suffix_tree");
      result = suffix_tree<uint>();
    }
  }
}
