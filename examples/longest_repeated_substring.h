#include <parlay/sequence.h>
#include <parlay/primitives.h>

#include "suffix_array.h"
#include "longest_common_prefix.h"

// **************************************************************
// Longest repeated substring in a string (allowed to overlap itself)
// returns
//  1) the length of the longest match
//  2) start of the first string in s
//  3) start of the second string in s
// **************************************************************
template <typename charseq>
auto longest_repeated_substring(charseq const &s) {
  auto sa = suffix_array(s);
  auto lcps = lcp(s, sa);
  long idx = parlay::max_element(lcps, std::less{}) - lcps.begin();
  return std::tuple{ lcps[idx], sa[idx], sa[idx+1]};
}
