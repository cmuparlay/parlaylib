#include <atomic>
#include <utility>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/speculative_for.h"
// **************************************************************
// Mininum Edit Distance of two sequences
// Uses dynamic program parallelizing across diagonals of
// bsize x bsize blocks.
// O(nm) work and O(n + m) span
// **************************************************************
template <typename Seq>
long minimum_edit_distance(const Seq& s1, const Seq& s2) {
  long n = s1.size();
  long m = s2.size();
  // ensure m <= n
  if (n < m) return minimum_edit_distance(s2,s1);
  long bsize = 100;
  long nb = (n-1)/bsize + 1;
  long mb = (m-1)/bsize + 1;

  auto ra = parlay::tabulate(n, [&] (int i) {return i+1;});
  auto rb = ra;
  auto c = parlay::tabulate(m, [&] (int i) {return i+1;});
  parlay::sequence<int> da(mb,0);
  auto db = da;

  // processes one n x m block
  // row_in is previous row, col_in is previous column, and diag
  // is the element at (-1,-1)
  auto do_block = [&] (int* row_in, int* row_out, auto s1, int n,
                       int* col_in, int* col_out, auto s2, int m,
                       int diag) {
    parlay::sequence<int> row(n);
    for (int i=0; i < n; i++)	row[i] = row_in[i];
    for (int j=0; j < m; j++) {
      int prev = col_in[j];
      for (int i=0; i < n; i++) {
        prev = ((s1[i] == s2[j])
                ? diag
                : 1 + std::min(row[i], prev));
        diag = row[i];
        row[i] = prev;
      }
      diag = col_in[j];
      col_out[j] = row[n-1];
    }
    for (int i=0; i < n; i++) row_out[i] = row[i]; };

  // processes diagonals of blocks, k is the diagonal number
  // parallel across the diagonal
  for (long k=0; k < nb + mb - 1; k++) {
    long start = (k < nb) ? 0 : k - nb + 1;
    long end = std::min(k+1, mb);
    parlay::parallel_for(start, end, [&] (long j) {
      auto i = k - j;
      int io = i*bsize; int jo = j*bsize;
      if (j < end-1) db[j] = c[jo + bsize - 1];
      do_block(ra.begin() + io, rb.begin() + io,
               s1.begin() + io, std::min(bsize, n - io),
               c.begin() + jo, c.begin() + jo,
               s2.begin() + jo, std::min(bsize, m - jo),
               (j == 0) ? io : da[j-1]);}, 1);
    std::swap(ra, rb);
    std::swap(da, db);
  }
  return ra[n-1];
}
