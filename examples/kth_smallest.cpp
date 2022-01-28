#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/io.h"
#include "helper/heap_tree.h"

// **************************************************************
// Calculate the kth_smallest element in an unsorted sequence
// Uses a linear-work randomized algorithm
// Fast in practice
// **************************************************************
template <typename Range,
	  typename Less = std::less<typename Range::value_type>>
auto kth_smallest(Range in, long k, Less less = {}) {
  long n = in.size();
  if (n <= 1000) return parlay::sort(in,less)[k];

  // pick 31 pivots by randomly choosing 8 * 31 keys, sorting them,
  // and taking every 8th key (i.e. oversampling)
  int sample_size = 31;
  int over = 8;
  parlay::random r;
  auto pivots = parlay::sort(parlay::tabulate(sample_size*over, [&] (long i) {return in[r[i]%n];}));
  pivots = parlay::tabulate(sample_size,[&] (long i) {return pivots[i*over];});

  // Determine which of the 32 buckets each key belongs in
  heap_tree ss(pivots);
  auto ids = parlay::tabulate(n, [&] (long i) -> unsigned char {
		      return ss.find(in[i], less);});

  // Count how many in keys are each bucket
  auto sums = parlay::histogram_by_index(ids, sample_size+1);

  // find which bucket k belongs in, and pack the keys in that bucket into next
  auto [offsets, total] = parlay::scan(sums);
  auto id = std::upper_bound(offsets.begin(), offsets.end(), k) - offsets.begin() - 1;
  auto next = parlay::pack(in, parlay::delayed_map(ids, [=] (auto b) {return b == id;}));

  // recurse on much smaller set, adjusting k as needed
  return kth_smallest(next, k - offsets[id], less);
}

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: kth_smallest <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random r;

    // generate random long values
    auto data = parlay::tabulate(n, [&] (long i) {return (long) (r[i]%n);});

    long result = kth_smallest(data, n/2);
    std::cout << "median is: " << result << std::endl;
  }
}
