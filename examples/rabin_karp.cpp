#include <iostream>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

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
    parlay::sequence<long> locations;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      locations = rabin_karp(str, search_str);
      t.next("rabin_karp");
    }

    std::cout << "total matches = " << locations.size() << std::endl;
    if (locations.size() > 10) {
      auto r = parlay::to_sequence(locations.cut(0,10));
      std::cout << "at locations: " << to_chars(r) << " ..." <<  std::endl;
    } else if (locations.size() > 0) {
      std::cout << "at locations: " << to_chars(locations) << std::endl;
    }
  }
}
