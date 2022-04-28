#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "word_counts.h"

// **************************************************************
// Driver code
// **************************************************************
using charseq = parlay::sequence<char>;

int main(int argc, char* argv[]) {
  auto usage = "Usage: word_counts <n> <filename>\nprints first <n> words.";
  if (argc != 3) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    charseq str = parlay::chars_from_file(argv[2]);

    // clean up by just keeping alphabetic chars, and lowercase them
    str = parlay::map(str, [] (unsigned char c) -> char {
      return std::isalpha(c) ? std::tolower(c) : ' '; });

    parlay::sequence<std::pair<parlay::chars, long>> counts;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      counts = word_counts(str);
      t.next("word_counts");
    }

    // take first n entries
    auto head = counts.head(std::min(n, (long) counts.size()));

    // format each line as word-space-count-newline
    auto b = parlay::map(head, [&] (auto&& w) {
      parlay::sequence<charseq> s = {
          w.first,
          parlay::to_chars(" "),
          parlay::to_chars(w.second),
          parlay::to_chars("\n")
      };
      return parlay::flatten(s);
    });
    std::cout << parlay::flatten(b);
  }
}
