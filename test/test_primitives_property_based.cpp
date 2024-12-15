#include <regex>
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
  auto comp = std::greater<>();
  std::sort(list1.begin(), list1.end(), comp);
  std::sort(list2.begin(), list2.end(), comp);
  auto actual = parlay::merge(list1, list2, comp);
  parlay::sequence<int> expected;
  std::merge(list1.begin(),list1.end(), list2.begin(), list2.end(),
             std::back_inserter(expected), comp);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testForEach,
              (parlay::sequence<int> list)) {
  parlay::sequence<int> seq(list.size());
  parlay::for_each(parlay::iota(list.size()), [&](int i) {
    seq[i] = list[i];
  });
  RC_ASSERT(seq == list);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testCountIf,
              (std::vector<bool> list)) {
  size_t expected = std::count_if(list.begin(), list.end(), [](bool b){return b;});
  size_t actual = parlay::count_if(list, [](bool b) {return b;});
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

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testFind,
              (const std::vector<int> &original, size_t num, bool exist)) {
  int search = exist ? (original.empty() ? 0 : original[num % original.size()]) : (int)num;
  auto expected = std::find(original.begin(), original.end(), search);
  auto actual = parlay::find(original, search);
  RC_ASSERT(expected == actual);
  // Test find if
  auto pred = [=](int x) {return x == search;};
  expected = std::find_if(original.begin(), original.end(), pred);
  actual = parlay::find_if(original, pred);
  RC_ASSERT(expected == actual);
  // Test find if not
  auto pred2 = [=](int x) {return std::abs(x) % 4 <= std::abs(int(num)) % 4; };
  expected = std::find_if_not(original.begin(), original.end(), pred2);
  actual = parlay::find_if_not(original, pred2);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testFindFirstOf,
              (std::vector<int> list1, std::vector<int> list2)) {
  if (list2.size() > 1000) {
    list2.resize(1000);
  }
  auto expected = std::find_first_of(list1.begin(), list1.end(), list2.begin(), list2.end());
  auto actual = parlay::find_first_of(list1, list2);
  RC_ASSERT(expected == actual);

  expected = std::find_end(list1.begin(), list1.end(), list2.begin(), list2.end());
  // FIXME: segfaults on line 708 of primitives.h when list1 is empty/smaller than list2
  RC_PRE(list1.size() >= list2.size());
  actual = parlay::find_end(list1, list2);
  RC_ASSERT(expected == actual);
}


RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMismatch,
              (const std::vector<int> &list1, const std::vector<int> &list2)) {
  auto expected = std::mismatch(list1.begin(), list1.end(), list2.begin(), list2.end());
  auto actual = parlay::mismatch(list1, list2);
  RC_ASSERT(expected == actual);
}


RC_GTEST_PROP(TestPrimitivePropertyBased,
              testEqual,
              (const std::vector<int> &list1, const std::vector<int> &list2)) {
  auto expected = std::equal(list1.begin(), list1.end(), list2.begin(), list2.end());
  auto actual = parlay::equal(list1, list2);
  RC_ASSERT(expected == actual);
}


RC_GTEST_PROP(TestPrimitivePropertyBased,
              testLexigraphicalCompare,
              (const std::vector<int> &list1, const std::vector<int> &list2)) {
  auto expected = std::lexicographical_compare(list1.begin(), list1.end(), list2.begin(), list2.end());
  auto actual = parlay::lexicographical_compare(list1, list2);
  RC_ASSERT(expected == actual);
}


RC_GTEST_PROP(TestPrimitivePropertyBased,
              testUnique,
              (std::vector<int> list1)) {
  auto actual = parlay::unique(list1);
  auto expected = std::unique(list1.begin(), list1.end());
  RC_ASSERT(parlay::sequence<int>(list1.begin(), expected) == actual);
}


RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMinMaxElement,
              (std::vector<int> list1)) {
  auto expected = std::min_element(list1.begin(), list1.end());
  auto actual = parlay::min_element(list1);
  RC_ASSERT(expected == actual);

  expected = std::max_element(list1.begin(), list1.end());
  actual = parlay::max_element(list1);
  RC_ASSERT(expected == actual);

  auto [expected1, expected2] = std::minmax_element(list1.begin(), list1.end());
  auto [actual1, actual2] = parlay::minmax_element(list1);
  if (expected1 != actual1) {
    RC_ASSERT(*expected1 == *actual1);
  }
  if (expected2 != actual2) {
    RC_ASSERT(*expected2 == *actual2);
  }
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

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testRotate,
              (std::vector<int> list1, size_t rotate)) {
  rotate = rotate % (list1.size() + 1);
  auto actual = parlay::rotate(list1, rotate);
  std::rotate(list1.begin(), list1.begin() + (long)rotate, list1.end());
  RC_ASSERT(parlay::sequence<int>(list1.begin(), list1.end()) == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testIsSorted,
              (std::vector<int> list1)) {
  auto expected = std::is_sorted(list1.begin(), list1.end());
  auto actual = parlay::is_sorted(list1);
  RC_ASSERT(expected == actual);

  parlay::sort_inplace(list1);
  RC_ASSERT(parlay::is_sorted(list1));
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testIsSortedUntil,
              (std::vector<int> list1)) {
  auto expected = std::is_sorted_until(list1.begin(), list1.end());
  auto actual = parlay::is_sorted_until(list1);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testIsPartitioned,
              (std::vector<int> list1)) {
  auto pred = [](int x) {return x % 2 == 0;};
  auto expected = std::is_partitioned(list1.begin(), list1.end(), pred);
  auto actual = parlay::is_partitioned(list1, pred);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testRemove,
              (std::vector<int> list1, int index)) {
  for (int value : {index, list1.empty() ? -1 : list1[index % list1.size()]}) {
    auto actual = parlay::remove(list1, value);
    auto expected = std::remove(list1.begin(), list1.end(), value);
    RC_ASSERT(std::equal(list1.begin(), expected, actual.begin()));
  }
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testRemoveIf,
              (std::vector<int> list1, int num)) {
  const int MOD = 4;
  num = std::abs(num) % MOD;
  auto pred = [=](int x) {return x % MOD < num;};
  auto actual = parlay::remove_if(list1, pred);
  auto expected = std::remove_if(list1.begin(), list1.end(), pred);
  RC_ASSERT(std::equal(list1.begin(), expected, actual.begin()));
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testFlattenSplitSame,
              (parlay::sequence<int> list1, int num)) {
  // Randomly split and then flatten. Should yield the same list.
  int mod = std::abs(num) % 103 + 1;
  auto pred = [=](int x) {return x % mod == 0; };
  auto bools = parlay::map(list1, pred);
  RC_ASSERT(parlay::flatten(parlay::split_at(list1, bools)) == list1);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testFlattenSplitSameDelayed,
              (parlay::sequence<int> list1, int num)) {
  auto map_function = [=](int x) {return x * num;};
  auto expected = parlay::map(list1, map_function);
  // Randomly split, map, and then flatten. Should be the same as mapping the original list.
  int mod = std::abs(num) % 103 + 1;
  auto pred = [=](int x) {return x % mod == 0; };
  auto segments = parlay::split_at(list1, parlay::delayed_map(list1, pred));
  auto actual = parlay::flatten(parlay::delayed_tabulate(segments.size(), [&](size_t i) {return parlay::delayed_map(segments[i], map_function); }));
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testTokens,
              (const std::string &str)) {
  std::regex words_regex("[^\\s]+");
  auto words_begin = std::sregex_iterator(str.begin(), str.end(), words_regex);
  auto words_end = std::sregex_iterator();
  auto expected = parlay::map(parlay::sequence<std::smatch>(words_begin, words_end), [](const auto& match) {
    return parlay::to_sequence(match.str());
  });
  auto actual = parlay::tokens(str);
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMapTokens,
              (const std::string &str)) {
  std::regex words_regex("[^\\s]+");
  auto words_begin = std::sregex_iterator(str.begin(), str.end(), words_regex);
  auto words_end = std::sregex_iterator();
  auto expected = parlay::map(parlay::sequence<std::smatch>(words_begin, words_end), [](const auto& match) {
    return match.str().length();
  });
  auto actual = parlay::map_tokens(str, [](auto s){return s.size();});
  RC_ASSERT(expected == actual);
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testMapTokensVoid,
              (const std::string &str)) {
  const int LIMIT = 256, OFFSET = 128;
  std::regex words_regex("[^\\s]+");
  auto words_begin = std::sregex_iterator(str.begin(), str.end(), words_regex);
  auto words_end = std::sregex_iterator();
  auto characters = parlay::flatten(parlay::map(parlay::sequence<std::smatch>(words_begin, words_end), [](const auto& match) {
    return parlay::to_sequence(match.str());
  }));
  auto frequencies = parlay::histogram_by_key(characters);
  std::map<char, int> expected_counts(frequencies.begin(), frequencies.end());
  std::array<std::atomic<int>, LIMIT> actual_counts{0};
  parlay::map_tokens(str, [&](auto s){
    for (char c : s) {
      actual_counts[c + OFFSET].fetch_add(1);
    }
  });
  for(int i = 0; i < LIMIT; i++) {
    int expected = expected_counts[i - OFFSET];
    int actual = actual_counts[i].load();
    RC_ASSERT(expected == actual);
  }
}

auto split_by_predicate(const parlay::sequence<int> &list, std::function<bool(int)> pred) {
  parlay::sequence<parlay::sequence<int>> result;
  parlay::sequence<int> current;
  for (int num : list) {
    current.push_back(num);
    if (pred(num)) {
      result.push_back(current);
      current.clear();
    }
  }
  result.push_back(current);
  return result;
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testSplitAt,
              (parlay::sequence<int> list1, int random)) {
  int mod = std::abs(random) % 103 + 1;
  auto pred = [=](int x) {return x % mod == 0; };
  auto flags = parlay::map(list1, pred);
  auto actual = parlay::split_at(list1, flags);
  auto expected = split_by_predicate(list1, pred);
  RC_ASSERT(expected == actual);

  auto map_function = [=](auto lst) {return std::accumulate(lst.begin(), lst.end(), 0); };
  auto expected2 = parlay::map(parlay::split_at(list1, flags), map_function);
  auto actual2 = parlay::map_split_at(list1, flags, map_function);
  RC_ASSERT(expected2 == actual2);
}



RC_GTEST_PROP(TestPrimitivePropertyBased,
              testRank,
              (const std::vector<int> &original)) {
  size_t n = original.size();
  auto list = parlay::sort(parlay::tabulate(n, [&](size_t index) -> std::pair<int, size_t> {return {original[index], index};}));
  auto ranks = parlay::rank(original);
  for (size_t i = 0; i < n; i++) {
    std::pair<int, size_t> target(original[i], i);
    size_t expected = std::distance(list.begin(), std::lower_bound(list.begin(), list.end(), target));
    RC_ASSERT(expected == ranks[i]);
  }
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testRankWithIota,
              (const std::vector<int> &v, int seed)) {
  // n shouldn't be too large, otherwise the test will take forever
  size_t n = v.size();
  auto list = parlay::map(parlay::iota(n), [](auto x) {return (unsigned long)x;});
  RC_ASSERT(list == parlay::rank(list));

  parlay::random random(seed);
  list = parlay::random_shuffle(list, random);
  RC_ASSERT(list == parlay::rank(list));
}

RC_GTEST_PROP(TestPrimitivePropertyBased,
              testKthSmallest,
              (std::vector<int> list, int seed)) {
  RC_PRE(!list.empty());
  size_t n = list.size();
  auto sorted = parlay::sort(list);
  parlay::random random(seed);
  // Randomly test 10 indices in search
  for (size_t i = 0; i < 10; i++) {
    auto index = random[i] % n;
    auto expected = sorted[index];
    auto actual = *parlay::kth_smallest(list, index);
    RC_ASSERT(expected == actual);
    actual = parlay::kth_smallest_copy(list, index);
    RC_ASSERT(expected == actual);
  }
}