#include "gtest/gtest.h"
#include "rapidcheck/gtest.h"

#include "parlay/primitives.h"
#include "parlay/delayed_sequence.h"

#include "rapid_check_utils.h"

RC_GTEST_PROP(TestPrimitivePropertyBased,
              tabulateSameAsIotaMap,
              (const std::vector<int> list)) {
  // Tabulate is the same as iota followed by a map
  auto n = list.size();
  auto f = [&](int i) {return list[i];};
  auto a1 = parlay::tabulate(n, f);
  auto a2 = parlay::map(parlay::iota(n), f);
  RC_ASSERT(a1 == a2);
  // Test delayed map as well
  auto seq = parlay::delayed_map(parlay::iota(n), f);
  parlay::sequence<int> a3(seq.begin(), seq.end());
  RC_ASSERT(a1 == a3);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testReduce,
              (const std::vector<int64_t> &list)) {
  int64_t reduce = parlay::reduce(list);
  RC_ASSERT(reduce == std::accumulate(list.begin(), list.end(), 0LL));

  // FIXME: segfaults for empty lists. Is this the expected behavior? Surely a runtime check is not that expensive.
  RC_PRE(!list.empty());

  int64_t reduce_max = parlay::reduce(list, parlay::maxm<int64_t>());
  RC_ASSERT(reduce_max == *std::max_element(list.begin(), list.end()));
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testScan,
              (const std::vector<long long> &s)) {
  // There must be at least one element
  RC_PRE(!s.empty());
  // Test scan
  auto [scan_result, total] = parlay::scan(s);
  auto partial_sums = parlay::sequence<long long>(s.size());
  std::partial_sum(std::begin(s), std::end(s) - 1, std::begin(partial_sums) + 1);
  RC_ASSERT(scan_result == partial_sums);
  RC_ASSERT(total == std::accumulate(std::begin(s), std::end(s), 0LL));
  // Test scan_inplace
  auto scan_result2 = parlay::sequence<long long>(s.begin(), s.end());
  auto total2 = parlay::scan_inplace(scan_result2);
  RC_ASSERT(scan_result == scan_result2);
  RC_ASSERT(total == total2);

  // Test scan inclusive
  scan_result = parlay::scan_inclusive(s);
  std::partial_sum(std::begin(s), std::end(s), std::begin(partial_sums));
  RC_ASSERT(scan_result == partial_sums);
  // Test scan inplace inclusive
  scan_result2 = parlay::sequence<long long>(s.begin(), s.end());
  parlay::scan_inclusive_inplace(scan_result2);
  RC_ASSERT(scan_result == scan_result2);
}

// Boolean functions

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testBoolFunctions,
              (const std::vector<bool> &list)) {
  bool all_of = parlay::all_of(list, [](auto x) {return x;}),
       none_of = parlay::none_of(list, [](auto x) {return x;}),
       neg_none_of = parlay::none_of(list, [](auto x) {return !x;}),
       any_of = parlay::any_of(list, [](auto x) {return x;});
  // All is true is equivalent to none is false
  RC_ASSERT(all_of == neg_none_of);
  // Either at least one is true or none is true
  RC_ASSERT(any_of ^ none_of);
}

// Permutations
RC_GTEST_PROP(TestPrimitivePropertyBased,
              testReverse,
              (const parlay::sequence<int> &seq)) {
  // Double reverse remains the same
  RC_ASSERT(parlay::reverse(parlay::reverse(seq)) == seq);
  // Test reverse_inplace against std::reverse
  auto seq2 = seq, seq3 = seq;
  // First reverse
  std::reverse(seq2.begin(), seq2.end());
  parlay::reverse_inplace(seq3);
  RC_ASSERT(seq2 == seq3);
  // reverse same as reverse_inplace
  RC_ASSERT(parlay::reverse(seq) == seq2);
  // Second reverse
  std::reverse(seq2.begin(), seq2.end());
  parlay::reverse_inplace(seq3);
  RC_ASSERT(seq2 == seq3);
  // Same as original
  RC_ASSERT(seq == seq2);
}
