#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "suffix_array.h"

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
  auto usage = "Usage: suffix_array <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    charseq str = parlay::chars_from_file(argv[1]);
    using index = unsigned int;
    long n = str.size();
    parlay::sequence<index> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = suffix_array(str);
      t.next("suffix_array");
    }

    // take first n entries
    auto cnt = std::min<long>(10, n);
    auto head = result.head(cnt);
    std::cout << "first 10 entries: " << to_chars(head) << std::endl;

    // check correctness if not too long
    if (n <= 1000000 && check(str) != result)
      std::cout << "check failed" << std::endl;
  }
}
