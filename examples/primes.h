#include <algorithm>
#include <cmath>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Parallel primes
// Returns primes up to n (inclusive).
// Based on primes sieve but designed to be cache efficient.
// In particular it sieves over blocks of size sqrt(n), which presumably fit in cache.
// It does O(n log log n) work with O(sqrt(n) log n) span.
// **************************************************************

parlay::sequence<long> primes(long n) {
  // base case
  if (n < 2) return parlay::sequence<long>();

  // recursively find primes up to sqrt(n)
  long sqrt_n = std::sqrt(n);
  auto sqrt_primes = primes(sqrt_n);

  // n+1 flags set to true that will be made false if a multiple of a prime
  parlay::sequence<bool> flags(n+1, true);

  // for each block of size sqrt(n) in parallel
  parlay::parallel_for(0, n/sqrt_n + 1, [&] (long i) {
    long start = sqrt_n * i;
    long end = (std::min)(start + sqrt_n, n+1);

    // for each prime up to sqrt(n)
    for (long j = 0; j < sqrt_primes.size(); j++) {
      long p = sqrt_primes[j];
      long first = (std::max)(2*p,(((start-1)/p)+1)*p);

      // for each multiple of the prime within the block
      // unset the flag
      for (long k = first; k < end; k += p)
        flags[k] = false;
    };}, 1); // the 1 means set granularity to 1 iteration

  // 0 and 1 are not prime
  flags[0] = flags[1] = false;

  // filter to keep indices that remain true (i.e. the primes)
  return parlay::filter(parlay::iota(n+1), [&] (long i) {
    return flags[i];});
}
