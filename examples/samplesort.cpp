#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>
#include <parlay/io.h>

// **************************************************************
// Sample sort
// Basically a generalization of quicksort to many pivots.
// In particular this code picks up to 256 pivots by randomly selecting them.
// It then puts the keys into buckets depending on which pivots
// they fall between and recursively sorts within the bucket.
// Makes use of a parlaylib bucket sort for the bucketing,
// and std::sort for the base case and sorting the pivots.
// **************************************************************

// An efficient search tree to store pivots for finding which bucket a key belongs in
// The tree is indexed like heaps (i.e. the children of i are at 2*i+1 and 2*i+2).
// Significantly more efficient than binary search since it avoids conditionals
// The number of pivots must be 2^i-1 (i.e. fully balanced tree)
struct search_tree {
private:
  long size;
  parlay::sequence<long> tree;
  long levels;

  // converts from sorted sequence into a "heap indexed" tree
  void to_balanced_tree(parlay::sequence<long> In,
			long root, long l, long r) {
    size_t n = r - l;
    size_t m = l + n / 2;
    tree[root] = In[m];
    if (n == 1) return;
    to_balanced_tree(In, 2 * root + 1, l, m);
    to_balanced_tree(In, 2 * root + 2, m + 1, r);
  }
public:
  // constructor
  search_tree(const parlay::sequence<long>& keys) :
    size(keys.size()), tree(parlay::sequence<long>(size)),
    levels(parlay::log2_up(size)) {
    to_balanced_tree(keys, 0, 0, size);}

  // finds a key in the "heap indexed" tree
  inline int find(long key) {
    long j = 0;
    for (size_t k = 0; k < levels; k++)
      j = 1 + 2 * j + !(key < tree[j]);
    return j - size;
  }
};

// Some glue to call an internal parlaylib counting sort.
// It sorts In into Out based on the small integer key values (no more than 256)
// It returns the offset in "out" for the start of each bucket
template <typename Range, typename Keys>
auto bucket_sort(Range in, Range out, Keys keys, long num_buckets) {
  return parlay::internal::count_sort<parlay::copy_assign_tag>(in, out, parlay::make_slice(keys),
							       num_buckets).first;
}

//  The main recursive algorithm
template <typename Range, typename Less>
void sample_sort_(Range in, Range out, bool inplace, Less less, int level=1) {
  long n = in.size();

  // for the base case (small or recursion level greater than 1) use std::sort
  if (n < 1024 || level > 2) {
    std::sort(in.begin(), in.end());
    if (!inplace) parlay::copy(in, out);
    return;
  }

  // number of bits in bucket count (e.g. 8 would mean 256 buckets)
  int bits = std::min(8ul, parlay::log2_up(n)-8);
  long num_buckets = 1 << bits;

  // over-sampling ratio: keeps the buckets more balanced
  int over_ratio = 4;

  // create an oversample and sort it using std:sort
  parlay::random r;
  auto oversample = parlay::tabulate(num_buckets * over_ratio, [&] (long i) {
						       return in[r[i]%n];});
  std::sort(oversample.begin(), oversample.end());

  // subsample to pick final pivots (num_buckets - 1 of them)
  auto pivots = parlay::tabulate(num_buckets-1, [&] (long i) {
						  return oversample[(i+1)*over_ratio];});

  // put pivots into efficient search tree and find buckets for all input keys
  search_tree ss(pivots);
  auto bucket_nums = parlay::tabulate(n, [&] (long i) -> unsigned char {
					   return ss.find(in[i]);});

  // sort into the buckets
  auto offsets = bucket_sort(in, out, bucket_nums, num_buckets);

  // now recursively sort each bucket
  parlay::parallel_for(0, num_buckets, [&] (long i) {
					 auto ins = in.cut(offsets[i], offsets[i+1]);
					 auto outs = out.cut(offsets[i], offsets[i+1]);
					 sample_sort_(outs, ins, !inplace, less, level+1);
				       }, 1);
}

//  A wraper that allocates a temporary sequence and calls sample_sort_
template <typename Range,
	  typename Less = std::less<typename Range::value_type>>
void sample_sort(Range& in, Less less = {}) {
  parlay::internal::timer t;
  long n = in.size();
  using T = typename Range::value_type;
  auto tmp = parlay::sequence<T>::uninitialized(n);
  sample_sort_(in.cut(0,n), tmp.cut(0,n), true, less);
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
    auto data = parlay::tabulate(n, [&] (long i) -> long {return r[i]%n;});

    parlay::sequence<long> result;
    for (int i=0; i < 7; i++) {
      result = data;
      sample_sort(result);
    }
    auto first_ten = result.head(10);
    std::cout << "first 10 elements: " << parlay::to_chars(first_ten) << std::endl;
  }
}
