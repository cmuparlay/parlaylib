#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "hash_map.h"

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "hash_map <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    using p = std::pair<long,long>;
    
    parlay::random_generator gen(0);
    std::uniform_int_distribution<long> dis(0,n);

    // generate n random keys
    auto pairs = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return p{dis(r),i};
    });

    parlay::sequence<long> keys;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
	hash_map<long,long> m(n);
	t.next("hash_map : construct");
	parlay::for_each(pairs, [&] (auto p) {
	    m.insert(p.first, p.second);});
	t.next("hash_map : insert");
	parlay::for_each(pairs, [&] (auto p) {
	    m.find(p.second);});
	t.next("hash_map : find");
    }

    std::cout << "number of unique keys: " << keys.size() << std::endl;
  }
}
