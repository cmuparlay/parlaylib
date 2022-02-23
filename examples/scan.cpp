#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>
#include <parlay/io.h>

// **************************************************************
// An implementation of scan.
// Uses standard contraction-based algorithm with blocking.
// Probably as fast, or close to, as the built in version.
// **************************************************************

template<typename Range, typename BinaryOp>
auto scan(const Range& A, const BinaryOp&& binop) {
  long n = A.size();
  long block_size = 100;
  using T = typename Range::value_type;
  // Avoids initializing.  Must use assign_uninitialized on it.
  auto r = parlay::sequence<T>::uninitialized(n);
  
  auto scan_seq = [&] (long start, long end, T init) {
    for (long i=start; i < end; i++) {
      parlay::assign_uninitialized(r[i], init);
      init = binop(init,A[i]);
    }
    return init;
  };
  
  if (n <= block_size)
    return std::pair{r, scan_seq(0, n, binop.identity)};
  else {
    long num_blocks = 1 + (n - 1) / block_size;

    // contract each block
    auto sums = parlay::tabulate(num_blocks, [&] (long i) {
	long start = i * block_size;
	T v = A[start];
	for (long i=start+1; i < std::min(start + block_size, n); i++)
	  v = binop(v, A[i]);
	return v;});

    // recursive call
    auto [part, total] = scan(sums, binop); 

    // expand back out
    parlay::parallel_for(0, num_blocks, [&, prt=part] (long i) {
	scan_seq(i*block_size, std::min((i+1)*block_size, n),
		 prt[i]);});
    return std::pair{std::move(r), total};
  }
}

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: scan <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::sequence ones(n,1);
    auto [result, total] = ::scan(ones, parlay::plus<int>());

    std::cout << "first 10 elements for scan on 1s: "
	      << parlay::to_chars(parlay::to_sequence(result.head(10)))
	      << std::endl;
  }
}
