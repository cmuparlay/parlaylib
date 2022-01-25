#include <parlay/primitives.h>
#include <parlay/io.h>
#include <cctype>

using charseq = parlay::sequence<char>;

// **************************************************************
// Counts the number times each space-separated word appears in a file
// Returns a sequenc or word-count pairs, in arbitrary order
// **************************************************************
auto word_counts(charseq const &str) {
  auto words = parlay::tokens(str, [] (char c) {return c == ' ';});
  return parlay::histogram_by_key(words); 
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: word_counts <n> <filename>\nprints first <n> words.";
  long n;
  if (argc != 3)
    std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    charseq str = parlay::chars_from_file(argv[2]);

    // clean up by just looking at alphabetic chars, and lowercase them
    str = parlay::map(str, [] (char c) -> char {
	    return std::isalpha(c) ? std::tolower(c) : ' ';});
    
    auto counts = word_counts(str);
    auto head = counts.head((std::min)(n, (long) counts.size()));

    // format each line as word-space-count-newline
    auto b = parlay::map(head, [&] (auto const &w) {
	       parlay::sequence<charseq> s = {
		  w.first,
		  parlay::to_chars(" "),
		  parlay::to_chars(w.second),
		  parlay::to_chars("\n")};
	       return parlay::flatten(s);
	     });
    std::cout << parlay::flatten(b);
  }
}
