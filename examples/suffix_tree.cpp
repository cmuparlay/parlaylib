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

int main(int argc, char* argv[]) {
  auto usage = "Usage: suffix_tree <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    charseq str = parlay::chars_from_file(argv[1]);
    using index = unsigned int;
    long n = str.size();
    radix_tree<index> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      result = radix_tree<index>();
      t.start();
      result = suffix_tree<index>(str);
      t.next("suffix_tree");
    }

    if (find(result, str, str) != 0)
      std::cout << "Error: string not found" << std::endl;
    else
      std::cout << "Found string in itself" << std::endl;
  }
}
