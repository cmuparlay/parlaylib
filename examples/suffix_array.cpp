#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "suffix_array.h"
//#include "hsa.h"

// **************************************************************
// Driver code
// **************************************************************
using charseq = parlay::sequence<char>;
using uint = unsigned int;

auto check(parlay::sequence<unsigned char>& str) {
  auto less = [&] (uint i, uint j) {
    return parlay::lexicographical_compare(str.cut(i,str.size()),str.cut(j,str.size()));};
  return parlay::sort(parlay::iota<uint>(str.size()), less);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: suffix_array <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    charseq str = parlay::chars_from_file(argv[1]);
    using index = unsigned int;
    auto ustr = parlay::map(str, [] (char x) {return (unsigned char) x;});
    long n = ustr.size();
    parlay::sequence<index> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = suffix_array(ustr);
      t.next("suffix_array");
    }

    // take first n entries
    auto cnt = std::min<long>(10, n);
    auto head = result.head(cnt);
    std::cout << "first 10 entries: " << to_chars(head) << std::endl;

    // check correctness if not too long
    if (n <= 1000000 && check(ustr) != result)
      std::cout << "check failed" << std::endl;
  }
}
