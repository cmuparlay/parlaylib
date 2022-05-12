#include <ctype.h>
#include <algorithm>
#include <iostream>
#include <string>

#include <parlay/io.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "radix_tree.h"

// **************************************************************
// Driver code
// **************************************************************
using charseq = parlay::sequence<char>;
using uint = unsigned int;

int main(int argc, char* argv[]) {
  auto usage = "Usage: radix_tree <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    charseq str = parlay::chars_from_file(argv[1]);

    // clean up by just keeping alphabetic chars, and lowercase them
    //str = parlay::map(str, [] (unsigned char c) -> char {
    //  return std::isalpha(c) ? std::tolower(c) : ' '; });

    // sort unique words
    auto words = parlay::tokens(str, [] (char c) {return std::isspace(c);}); //c == ' ';});
    words = parlay::sort(parlay::remove_duplicates(words));

    auto lcps = parlay::tabulate(words.size()-1, [&] (long i) {
	uint j=0;
	while (j < words[i].size() && j < words[i+1].size() &&
	       words[i][j] == words[i+1][j]) j++;
	return j;
      });

    radix_tree<uint> result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      t.start();
      result = radix_tree<uint>(lcps);
      t.next("radix_tree");
      result = radix_tree<uint>();
    }
    std::cout << "built radix tree on " << words.size() << " unique words" << std::endl;
  }
}
