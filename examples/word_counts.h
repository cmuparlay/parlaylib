#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Counts the number times each space-separated word appears in a file.
// Returns a sequence or word-count pairs, sorted by word frequency, max first
// **************************************************************

auto word_counts(parlay::sequence<char> const &str) {
  auto words = parlay::tokens(str, [] (char c) {return c == ' ';});
  auto pairs = parlay::histogram_by_key<long>(words);
  auto cmp = [] (auto const& a, auto const& b) {
    return a.second > b.second;};
  return parlay::sort(pairs, cmp);
}
