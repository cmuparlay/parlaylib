#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "longest_repeated_substring.h"

// **************************************************************
// Driver code
// **************************************************************
using uint = unsigned int;

int main(int argc, char* argv[]) {
  auto usage = "Usage: longest_repeated_substring <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    auto str = parlay::chars_from_file(argv[1]);
    using index = unsigned int;
    long n = str.size();
    auto SA = suffix_array(str);
    std::tuple<uint,uint,uint> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = longest_repeated_substring(str);
      t.next("longest_repeated_substring");
    }

    auto [len, l1, l2] = result;
    std::cout << "longest match has length = " << len
              << " at positions " << l1 << " and " << l2 << std::endl;
    std::cout << to_chars(to_sequence(str.cut(l1, l1 + std::min(2000u,len)))) << std::endl;
    if (len > 2000) std::cout << "...." << std::endl;
  }
}
