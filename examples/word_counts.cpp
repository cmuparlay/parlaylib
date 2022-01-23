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
  long n;
  if (argc != 3)
    std::cout << "word_counts <n> <filename>" << std::endl;
  else {
    // should catch invalid argument exception if not an integer
    n = std::stol(argv[1]);
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
