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
              testCopy,
              (const parlay::sequence<int> &list)) {
  parlay::sequence<int> list2(list.size());
  parlay::copy(list, list2);
  RC_ASSERT(list == list2);
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
              testReduceBool,
              (const parlay::sequence<bool> &list)) {
  parlay::monoid monoid([](int x, int y) {return x+y;}, 0);
  int actual = parlay::reduce(list, monoid);
  int expected = parlay::reduce(parlay::map(list, [](bool b) {return (int)b;}));
  RC_ASSERT(expected == actual);
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

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testPack,
              (parlay::sequence<int> list1,
               parlay::sequence<bool> list2)) {
  size_t size = std::min(list1.size(), list2.size());
  // FIXME: test fails/segfaults when list1 and list2 have different sizes
  auto actual = parlay::pack(list1.cut(0, size), list2.cut(0, size));
  auto expected = parlay::sequence<int>();
  for (size_t i = 0; i < size; i++) {
    if (list2[i]) {
      expected.push_back(list1[i]);
    }
  }
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testPackSameAsFilter,
              (const parlay::sequence<int> &list, int mod, int threshold)) {
  RC_PRE(mod != 0);
  threshold %= mod;
  auto f = [=](int num) {return num % mod < threshold;};
  auto expected = parlay::filter(list, f);
  auto bool_list = parlay::map(list, f);
  auto actual = parlay::pack(list, bool_list);
  RC_ASSERT(expected == actual);
  // Test pack_into_uninitialized
  int count = parlay::reduce(bool_list, parlay::monoid([](int a, int b){return a + b;}, 0));
  actual = parlay::sequence<int>::uninitialized(count);
  parlay::pack_into_uninitialized(list, bool_list, actual);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testPackIndex,
              (const parlay::sequence<bool> &list)) {
  auto actual = parlay::pack_index(list);
  auto expected = parlay::pack(parlay::iota(list.size()), list);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testFilter,
              (const std::vector<int> &list, uint8_t threshold)) {
  auto f = [=](int x){return x % 255 < threshold; };
  auto actual = parlay::filter(list, f);
  parlay::sequence<int> expected;
  std::copy_if(list.begin(), list.end(), std::back_inserter(expected), f);
  RC_ASSERT(expected == actual);
  // Filter is idempotent
  RC_ASSERT(actual == parlay::filter(actual, f));
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMerge,
              (std::vector<int> list1, std::vector<int> list2)) {
  std::sort(list1.begin(), list1.end());
  std::sort(list2.begin(), list2.end());
  auto actual = parlay::merge(list1, list2);
  parlay::sequence<int> expected;
  std::merge(list1.begin(),list1.end(), list2.begin(), list2.end(),
             std::back_inserter(expected));
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMergeCustomComparator,
              (std::vector<int> list1, std::vector<int> list2)) {
  auto comp = std::greater<int>();
  std::sort(list1.begin(), list1.end(), comp);
  std::sort(list2.begin(), list2.end(), comp);
  auto actual = parlay::merge(list1, list2, comp);
  parlay::sequence<int> expected;
  std::merge(list1.begin(),list1.end(), list2.begin(), list2.end(),
             std::back_inserter(expected), comp);
  RC_ASSERT(expected == actual);
}

// Boolean functions

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testBoolFunctions,
              (const std::vector<bool> &list)) {
  auto identity = [](auto x) {return x;};
  bool all_of = parlay::all_of(list, identity),
       none_of = parlay::none_of(list, identity),
       neg_none_of = parlay::none_of(list, [](auto x) {return !x;}),
       any_of = parlay::any_of(list, identity);
  // All is true is equivalent to none is false
  RC_ASSERT(all_of == neg_none_of);
  // Either at least one is true or none is true
  RC_ASSERT(any_of ^ none_of);

  RC_ASSERT(all_of == std::all_of(list.begin(), list.end(), identity));
  RC_ASSERT(any_of == std::any_of(list.begin(), list.end(), identity));
  RC_ASSERT(none_of == std::none_of(list.begin(), list.end(), identity));
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
