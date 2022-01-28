#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/io.h"

// **************************************************************
// Sample sort
// Basically a generalization of quicksort to many pivots.
// In particular this code picks up to 256 pivots by randomly selecting them.
// It then puts the keys into buckets depending on which pivots
// they fall between and recursively sorts within the buckets.
// Makes use of a parlaylib bucket sort for the bucketing,
// and std::sort for the base case and sorting the pivots.
// **************************************************************

// An efficient search tree to store pivots for finding which bucket a key belongs in
// The tree is indexed like heaps (i.e. the children of i are at 2*i+1 and 2*i+2).
// Significantly more efficient than binary search since it avoids conditionals
// The number of pivots must be 2^i-1 (i.e. fully balanced tree)
template <typename T>
struct search_tree {
private:
  const long size;
  parlay::sequence<T> tree;
  const long levels;

  // converts from sorted sequence into a "heap indexed" tree
  void to_tree(const parlay::sequence<T>& In,
	       long root, long l, long r) {
    size_t n = r - l;
    size_t m = l + n / 2;
    tree[root] = In[m];
    if (n == 1) return;
    to_tree(In, 2 * root + 1, l, m);
    to_tree(In, 2 * root + 2, m + 1, r);
  }
public:
  // constructor
  search_tree(const parlay::sequence<T>& keys) :
    size(keys.size()), tree(parlay::sequence<T>(size)),
    levels(parlay::log2_up(keys.size()+1)-1) {
    to_tree(keys, 0, 0, size);}

  // finds a key in the "heap indexed" tree
  template <typename Less>
  inline int find(const T& key, const Less& less) {
    long j = 0;
    for (int k = 0; k < levels; k++) {
      j = 1 + 2 * j + less(tree[j],key);
    }
    j = 1 + 2 * j + !less(key,tree[j]);
    return j - size;
  }
};

// **************************************************************
//  The main recursive algorithm
// **************************************************************
template <typename Range, typename Less>
void sample_sort_(Range in, Range out, Less less, int level=1) {
  long n = in.size();

  // for the base case (small or recursion level greater than 1) use std::sort
  long cutoff = 256;
  if (n <= cutoff || level > 2) {
    parlay::copy(in, out);
    std::sort(out.begin(), out.end());
    return;
  }

  // number of bits in bucket count (e.g. 8 would mean 256 buckets)
  int bits = std::min(8ul, parlay::log2_up(n)-parlay::log2_up(cutoff)+1);
  long num_buckets = 1 << bits;

  // over-sampling ratio: keeps the buckets more balanced
  int over_ratio = 8;

  // create an over sample and sort it using std:sort
  parlay::random r;
  auto oversample = parlay::tabulate(num_buckets * over_ratio, [&] (long i) {
		      return in[r[i]%n];});
  std::sort(oversample.begin(), oversample.end());
  
  // sub sample to pick final pivots (num_buckets - 1 of them)
  auto pivots = parlay::tabulate(num_buckets-1, [&] (long i) {
		  return oversample[(i+1)*over_ratio];});
    
  // put pivots into efficient search tree and find buckets id for the input keys
  search_tree ss(pivots);
  auto bucket_ids = parlay::tabulate(n, [&] (long i) -> unsigned char {
		      return ss.find(in[i], less);});

  // sort into the buckets
  auto [keys,offsets] = parlay::internal::count_sort(in, bucket_ids, num_buckets);
  
  // now recursively sort each bucket
  parlay::parallel_for(0, num_buckets, [&] (long i) {
    long first = offsets[i];
    long last = offsets[i+1];
    if (last-first == 0) return; // empty
    // are all equal, then can copy and quit
    if (i == 0 || i == num_buckets - 1 || less(pivots[i - 1], pivots[i]))
	sample_sort_(keys.cut(first,last), out.cut(first,last), less, level+1);
    else parlay::copy(keys.cut(first,last), out.cut(first,last));}, 1);
}

//  A wraper that allocates a temporary sequence and calls sample_sort_
template <typename Range,
	  typename Less = std::less<typename Range::value_type>>
void sample_sort(Range& in, Less less = {}) {
  parlay::internal::timer t;
  auto ins = parlay::make_slice(in);
  sample_sort_(ins, ins, less);
  t.next("sort");
}

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: samplesort <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random r;

    // generate random long values
    auto data = parlay::tabulate(n, [&] (long i) {return (long) (r[i]%n);});

    auto result = data;
    for (int i=0; i < 7; i++) {
      result = data;
      sample_sort(result);
    }
    auto first_ten = result.head(10);
    std::cout << "first 10 elements: " << parlay::to_chars(first_ten) << std::endl;
  }
}
