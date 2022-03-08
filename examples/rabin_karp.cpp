#include <iostream>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "rabin_karp.h"

// **************************************************************
// Driver code
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: rabin_karp <search_string> <filename>";
  if (argc != 3) std::cout << usage << std::endl;
  else {
    parlay::chars str = parlay::chars_from_file(argv[2]);
    parlay::chars search_str = parlay::to_chars(argv[1]);
    long loc;
    loc = rabin_karp(str, search_str);
    if (loc < str.size())
      std::cout << "found at position: " << loc << std::endl;
    else std::cout << "not found" << std::endl;
  }
}
