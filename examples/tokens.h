#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Breaks a string (arbitrary range) into tokens identified by a 
// is_space predicate.
// Returns a sequence of whatever the input type is.
// tokens is a built-in function in parlay, and this is a possible
// implementation.
// **************************************************************

template <typename Range, typename F> auto tokens(const Range& str, F is_space) {
  long n = str.size();

  // checks if an index is at the start or one past the end of a token
  auto check = [&] (long i) {
    if (i == n) return !is_space(str[n-1]);
    else if (i == 0) return !is_space(str[0]);
    else {
      bool i1 = is_space(str[i]);
      bool i2 = is_space(str[i-1]);
      return (i1 && !i2) || (i2 && !i1);
    }};

  // generate indices of first and last locations
  // note that one past the end cound be n+1
  auto ids = parlay::filter(parlay::iota(n+1), check);

  // pair them up and extract subsequence
  return parlay::tabulate(ids.size()/2, [&] (long i) {
    return parlay::to_sequence(str.cut(ids[2*i],ids[2*i+1]));});
}
