#include <algorithm>

#include <parlay/sequence.h>
#include <parlay/primitives.h>

template <typename chars>
auto knuth_morris_pratt(const chars& str, const chars& pattern) {
  long n = pattern.size();
  long m = str.size();

  if(n == 0 || m == 0 || n > m)
    return parlay::sequence<long>();

  parlay::sequence<long> failure(n, -1);

  // process the pattern
  for(int r=1, l=-1; r<pattern.size(); r++) {
    while(l != -1 && pattern[l+1] != pattern[r])
      l = failure[l];
    if(pattern[l+1] == pattern[r])
      failure[r] = ++l;
  }

  // search the string in blocks, in parallel
  // for each block report hits in range, and flatten results across blocks
  long block_len = std::max(n, 1000l);
  long num_blocks = (m - 1)/block_len + 1;
  auto failure_p = failure.begin();
  auto str_p = str.begin();
  auto pattern_p = pattern.begin();
  return parlay::flatten(parlay::tabulate(num_blocks, [&] (long k) {
    long tail = -1;
    long start = k * block_len;
    long end = std::min(start + block_len, m);
    long end2 = std::min(start + block_len + n, m);
    parlay::sequence<long> out;
    for (int i=start; i < end2 && (i-tail) <= end; i++) {
      while (tail != -1 && str_p[i] != pattern_p[tail+1])
        tail = failure_p[tail];
      if (str_p[i] == pattern_p[tail+1]) tail++;
      if (tail == n-1) out.push_back(i - tail);
    }
    return out;}));
}
