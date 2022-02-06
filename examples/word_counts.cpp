#include <cctype>

#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Counts the number times each space-separated word appears in a file.
// Returns a sequence or word-count pairs, sorted by word frequency, max first
// **************************************************************

using charseq = parlay::sequence<char>;

auto word_counts(charseq const &str) {
  auto words = parlay::tokens(str, [] (char c) {return c == ' ';});
  auto pairs = parlay::histogram_by_key(words);
  auto cmp = [] (auto const& a, auto const& b) {
    return a.second > b.second;};
  return parlay::sort(pairs, cmp);
}

// **************************************************************
// Driver code
// **************************************************************
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

    auto counts = word_counts(str);

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
