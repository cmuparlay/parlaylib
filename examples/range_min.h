#include <parlay/parallel.h>
#include <parlay/primitives.h>

// **************************************************************
// Range Minima 
// builds a static range minima query structure:
//  range_min(a, less = std::less<>(), block_size = 32)
//    builds the structure on the sequence a based less
//  query(i,j) finds the minimum in the range from i to j inclusive of both
//    and returns its index
// Assuming less takes constant time:
//   Build takes O(n log n / block_size) time
//   Query takes O(block_size) time
// **************************************************************
template <typename Seq, typename Less = std::less<typename Seq::value_type>,
    typename Uint=unsigned int>
class range_min {
 public:
  range_min(Seq &a, Less&& less = {}, long block_size=32)
      :  a(a), less(less), n(a.size()), block_size(block_size) {
    if (a.size() == 0) return;
    m = 1 + (n-1)/block_size;
    preprocess();
  }

  Uint query(Uint i, Uint j) {
    long block_i = i/block_size;
    long block_j = j/block_size;

    // same or adjacent blocks
    if (block_i >= block_j - 1) {
      Uint r = i;
      for (long k = i+1; k <= j; k++)
        r = min_index(r, k);
      return r;
    }

    // min suffix of first block
    Uint minl = i;
    for (long k = minl+1; k < (block_i+1) * block_size; k++)
      minl = min_index(minl, k);

    //min prefix of last block
    long minr = block_j * block_size;
    for (long k = minr+1; k <= j; k++)
      minr = min_index(minr, k);

    // min between first and last
    Uint between_min;
    block_i++;
    block_j--;
    if (block_j == block_i)
      between_min = table[0][block_i];
    else if(block_j == block_i + 1)
      between_min = table[1][block_i];
    else {
      long k = parlay::log2_up(block_j - block_i + 1) - 1;
      long p = 1 << k; //2^k
      between_min = min_index(table[k][block_i], table[k][block_j+1-p]);
    }
    return min_index(minl, min_index(between_min, minr));
  }

 private:
  Seq &a;
  parlay::sequence<parlay::sequence<Uint>> table;
  Less less;
  long n, m, depth, block_size;

  Uint min_index(Uint i, Uint j) {
    return less(a[j], a[i]) ? j : i;}

  void preprocess() {
    depth = parlay::log2_up(m+1);

    // minimums within each block
    table.push_back(parlay::tabulate(m, [&] (long i) {
      Uint k = i * block_size;
      for (Uint j = k+1; j < std::min((i+1)*block_size, n); j++)
        k = min_index(j, k);
      return k;}));

    // minimum across increasing power of two blocks: 2, 4, 8, ...
    long dist = 1;
    for (long j = 1; j < depth; j++) {
      table.push_back(parlay::tabulate(m, [&] (long i) {
        return ((i < m - dist)
                ? min_index(table[j-1][i], table[j-1][i+dist])
                : table[j-1][i]);}));
      dist*=2;
    }
  }
};

