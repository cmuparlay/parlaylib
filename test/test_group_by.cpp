#include "gtest/gtest.h"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <parlay/primitives.h>

#include "sorting_utils.h"

class TestGroupByP : public testing::TestWithParam<size_t> { };

// -----------------------------------------------------------------------
//                          group_by_key_sorted
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestGroupByKeySorted) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_key_ordered(a);

  ASSERT_EQ(grouped.size(), 3);
  ASSERT_EQ(grouped[0].first, 1);
  ASSERT_EQ(grouped[1].first, 2);
  ASSERT_EQ(grouped[2].first, 3);
  ASSERT_EQ(grouped[0].second.size(), 3);
  ASSERT_EQ(grouped[1].second.size(), 5);
  ASSERT_EQ(grouped[2].second.size(), 3);
}

TEST_P(TestGroupByP, TestGroupByKeySortedLarge) {
  auto s = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_key_ordered(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    if (i > 0) { ASSERT_LT(result[i-1], result[i]); }
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);

    for (const auto& v : result[i].second) {
      ASSERT_EQ(bucket, v % num_buckets);
    }
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<unsigned long long> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}



TEST_P(TestGroupByP, TestGroupByKeySortedNonContiguous) {
  auto ss = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_key_ordered(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    if (i > 0) { ASSERT_LT(result[i-1], result[i]); }
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);

    for (const auto& v : result[i].second) {
      ASSERT_EQ(bucket, v % num_buckets);
    }
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<unsigned long long> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByKeySortedNonTrivial) {
  auto s = parlay::tabulate(20000, [](unsigned long long i) {
    // Pad the beginning to make sure they are not small-string optimized. We
    // want the objects to be non-trivial to copy.
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(std::to_string(std::stoull(x) % num_buckets), x); } );
  auto result = parlay::group_by_key_ordered(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    if (i > 0) { ASSERT_LT(result[i-1], result[i]); }
    size_t bucket = std::stoull(result[i].first);
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return std::stoull(x) % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<std::string> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<std::string>(std::begin(s), std::end(s)));
}


TEST_P(TestGroupByP, TestGroupByKeySortedNonRelocatable) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x.x % num_buckets, x); } );
  auto result = parlay::group_by_key_ordered(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    if (i > 0) { ASSERT_LT(result[i-1], result[i]); }
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x.x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<SelfReferentialThing> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<SelfReferentialThing>(std::begin(s), std::end(s)));
}

// -----------------------------------------------------------------------
//                          reduce_by_key
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestReduceByKey) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };

  auto reduced = parlay::reduce_by_key(a);

  ASSERT_EQ(reduced.size(), 3);
  std::map<int,int> results;

  for (const auto& result : reduced) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1], 42);
  ASSERT_EQ(results[2], 113);
  ASSERT_EQ(results[3], 105);
}

// Sum all of the even and odd numbers in a psuedorandom sequence
// using reduce by key, where the key is the parity
TEST_P(TestGroupByP, TestReduceByKeyLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::reduce_by_key(key_vals, parlay::plus<unsigned long long>{});

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    unsigned long long sum = 0;
    for (const auto& x : s) if (x % num_buckets == bucket) sum += x;
    ASSERT_EQ(result[i].second, sum);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);
}

TEST_P(TestGroupByP, TestReduceByKeyNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::reduce_by_key(key_vals, parlay::plus<unsigned long long>{});

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    unsigned long long sum = 0;
    for (const auto& x : s) if (x % num_buckets == bucket) sum += x;
    ASSERT_EQ(result[i].second, sum);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);
}

// Reduce a bunch of strings by concatenating them
TEST_P(TestGroupByP, TestReduceByKeyNonTrivial) {
  auto s = parlay::tabulate(10000, [](unsigned long long i)  {
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(std::stoull(x) % num_buckets, x); } );

  struct string_concat_monoid {
    std::string identity{};
    auto operator()(const std::string& a, const std::string& b) const { return a + b; }
  };

  auto result = parlay::reduce_by_key(key_vals, string_concat_monoid{});
  ASSERT_LE(result.size(), num_buckets);

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);
}

TEST_P(TestGroupByP, TestReduceByKeyNonRelocatable) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 10));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x.x % num_buckets, x); } );
  struct add_monoid {
    SelfReferentialThing identity{0};
    SelfReferentialThing operator()(const SelfReferentialThing& a, const SelfReferentialThing& b) const {
      return SelfReferentialThing(a.x + b.x);
    }
  };
  auto result = parlay::reduce_by_key(key_vals, add_monoid{});

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    unsigned long long sum = 0;
    for (const auto& x : s) if (x.x % num_buckets == bucket) sum += x.x;
    ASSERT_EQ(result[i].second, sum);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);
}


// -----------------------------------------------------------------------
//                             group_by_key
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestGroupByKey) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_key(a);

  ASSERT_EQ(grouped.size(), 3);
  std::map<int, parlay::sequence<int>> results;

  for (const auto& result : grouped) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1].size(), 3);
  ASSERT_EQ(results[2].size(), 5);
  ASSERT_EQ(results[3].size(), 3);
}

TEST_P(TestGroupByP, TestGroupByKeyLarge) {
  auto s = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_key(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<unsigned long long> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}



TEST_P(TestGroupByP, TestGroupByKeyNonContiguous) {
  auto ss = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_key(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<unsigned long long> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByKeyNonTrivial) {
  auto s = parlay::tabulate(20000, [](unsigned long long i) {
    // Pad the beginning to make sure they are not small-string optimized. We
    // want the objects to be non-trivial to copy.
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(std::to_string(std::stoull(x) % num_buckets), x); } );
  auto result = parlay::group_by_key(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = std::stoull(result[i].first);
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return std::stoull(x) % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<std::string> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<std::string>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByKeyNonRelocatable) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x.x % num_buckets, x); } );
  auto result = parlay::group_by_key(key_vals);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x.x % num_buckets == bucket; });
    ASSERT_GE(num, 1);
    ASSERT_EQ(result[i].second.size(), num);
  }

  std::set<decltype(result[0].first)> keys;
  for (const auto& kv : key_vals) keys.insert(std::get<0>(kv));
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keys, ret_keys);

  std::multiset<SelfReferentialThing> values;
  for (const auto& kv : result) for (const auto& v : kv.second) values.insert(v);
  ASSERT_EQ(values, std::multiset<SelfReferentialThing>(std::begin(s), std::end(s)));
}

// -----------------------------------------------------------------------
//                           histogram_by_key
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestHistogramByKey) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto counts = parlay::histogram_by_key(a);

  ASSERT_EQ(counts.size(), 3);
  std::map<int, int> results;

  for (const auto& result : counts) {
    results.insert(result);
  }

  ASSERT_EQ(results.count(1), 1);
  ASSERT_EQ(results.count(2), 1);
  ASSERT_EQ(results.count(3), 1);

  ASSERT_EQ(results[1], 3);
  ASSERT_EQ(results[2], 5);
  ASSERT_EQ(results[3], 3);
}

TEST_P(TestGroupByP, TestHistogramByKeyLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](auto x) { return x % num_buckets; });
  auto result = parlay::histogram_by_key(keys);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                           [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(num, result[i].second);
  }

  std::set<decltype(result[0].first)> keyset;
  for (const auto& k : keys) keyset.insert(k);
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keyset, ret_keys);
}

TEST_P(TestGroupByP, TestHistogramByKeyNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](auto x) { return x % num_buckets; });
  auto result = parlay::histogram_by_key(keys);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(num, result[i].second);
  }

  std::set<decltype(result[0].first)> keyset;
  for (const auto& k : keys) keyset.insert(k);
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keyset, ret_keys);
}

TEST_P(TestGroupByP, TestHistogramByKeyNonTrivial) {
  auto s = parlay::tabulate(20000, [](unsigned long long i) {
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](const auto& x) { return std::to_string(std::stoull(x) % num_buckets); });
  auto result = parlay::histogram_by_key(keys);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = std::stoull(result[i].first);
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return std::stoull(x) % num_buckets == bucket; });
    ASSERT_EQ(num, result[i].second);
  }

  std::set<decltype(result[0].first)> keyset;
  for (const auto& k : keys) keyset.insert(k);
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keyset, ret_keys);
}

TEST_P(TestGroupByP, TestHistogramByKeyNonRelocatable) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 20));
  });
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](auto x) { return SelfReferentialThing(x.x % num_buckets); });
  auto result = parlay::histogram_by_key(keys);

  ASSERT_LE(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = result[i].first.x;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x.x % num_buckets == bucket; });
    ASSERT_EQ(num, result[i].second);
  }

  std::set<decltype(result[0].first)> keyset;
  for (const auto& k : keys) keyset.insert(k);
  std::set<decltype(result[0].first)> ret_keys;
  for (const auto& kv : result) ASSERT_TRUE(ret_keys.insert(std::get<0>(kv)).second);
  ASSERT_EQ(keyset, ret_keys);
}

// -----------------------------------------------------------------------
//                             remove_duplicates
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestRemoveDuplicates) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto deduped = parlay::remove_duplicates(a);

  ASSERT_EQ(deduped.size(), 3);
  ASSERT_EQ(std::set<int>(deduped.begin(), deduped.end()), (std::set<int>{1,2,3}));
}

TEST(TestGroupBy, TestRemoveDuplicatesLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto deduped = parlay::remove_duplicates(s);

  ASSERT_EQ(deduped.size(), std::set<int>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<int>(s.begin(), s.end()), (std::set<int>(deduped.begin(), deduped.end())));
}

TEST(TestGroupBy, TestRemoveDuplicatesNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  auto deduped = parlay::remove_duplicates(s);

  ASSERT_EQ(deduped.size(), std::set<int>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<int>(s.begin(), s.end()), (std::set<int>(deduped.begin(), deduped.end())));
}

TEST(TestGroupBy, TestRemoveDuplicatesNonTrivial) {
  auto s = parlay::tabulate(20000, [](unsigned long long i) {
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  auto deduped = parlay::remove_duplicates(s);

  ASSERT_EQ(deduped.size(), std::set<std::string>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<std::string>(s.begin(), s.end()), (std::set<std::string>(deduped.begin(), deduped.end())));
}

TEST(TestGroupBy, TestRemoveDuplicatesNonRelocatable) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 20));
  });
  auto deduped = parlay::remove_duplicates(s);

  ASSERT_EQ(deduped.size(), std::set<SelfReferentialThing>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<SelfReferentialThing>(s.begin(), s.end()), (std::set<SelfReferentialThing>(deduped.begin(), deduped.end())));
}

// -----------------------------------------------------------------------
//                           reduce_by_index
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestReduceByIndex) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };

  auto reduced = parlay::reduce_by_index(a, 4);

  ASSERT_EQ(reduced.size(), 4);

  ASSERT_EQ(reduced[0], 0);
  ASSERT_EQ(reduced[1], 42);
  ASSERT_EQ(reduced[2], 113);
  ASSERT_EQ(reduced[3], 105);
}

TEST_P(TestGroupByP, TestReduceByIndexLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::reduce_by_index(key_vals, num_buckets, parlay::plus<unsigned long long>{});

  ASSERT_EQ(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    unsigned long long sum = 0;
    for (const auto& x : s) if (x % num_buckets == bucket) sum += x;
    ASSERT_EQ(result[i], sum);
  }
}

TEST_P(TestGroupByP, TestReduceByIndexNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::reduce_by_index(key_vals, num_buckets, parlay::plus<unsigned long long>{});

  ASSERT_EQ(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    unsigned long long sum = 0;
    for (const auto& x : s) if (x % num_buckets == bucket) sum += x;
    ASSERT_EQ(result[i], sum);
  }
}

TEST_P(TestGroupByP, TestReduceByIndexNonTrivial) {
  auto s = parlay::tabulate(10000, [](unsigned long long i)  {
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(std::stoull(x) % num_buckets, x); } );

  struct string_concat_monoid {
    std::string identity{};
    auto operator()(const std::string& a, const std::string& b) const { return a + b; }
  };

  auto result = parlay::reduce_by_index(key_vals, num_buckets, string_concat_monoid{});
  ASSERT_EQ(result.size(), num_buckets);
}

// -----------------------------------------------------------------------
//                           histogram
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestHistogramByIndex) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto counts = parlay::histogram_by_index(a, 4);

  ASSERT_EQ(counts.size(), 4);

  ASSERT_EQ(counts[0], 0);
  ASSERT_EQ(counts[1], 3);
  ASSERT_EQ(counts[2], 5);
  ASSERT_EQ(counts[3], 3);
}

TEST_P(TestGroupByP, TestHistogramByIndexLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](auto x) { return x % num_buckets; });
  auto result = parlay::histogram_by_index(keys, num_buckets);

  ASSERT_EQ(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(num, result[i]);
  }
}

TEST_P(TestGroupByP, TestHistogramByIndexNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  auto num_buckets = GetParam();
  auto keys = parlay::map(s, [num_buckets](auto x) { return x % num_buckets; });
  auto result = parlay::histogram_by_index(keys, num_buckets);

  ASSERT_EQ(result.size(), num_buckets);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(num, result[i]);
  }
}

// -----------------------------------------------------------------------
//                      remove_duplicates_by_index
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestRemoveDuplicateIntegers) {
  std::vector<int> a = {
      3,
      1,
      3,
      2,
      2,
      3,
      2,
      2,
      1,
      2,
      1
  };
  auto deduped = parlay::remove_duplicate_integers(a, 4);

  ASSERT_EQ(deduped.size(), 3);
  ASSERT_EQ(std::set<int>(deduped.begin(), deduped.end()), (std::set<int>{1,2,3}));
}

TEST(TestGroupBy, TestRemoveDuplicateIntegersLarge) {
  auto s = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return ((50021 * i + 61) % (1 << 20)) % 1000;
  });
  auto deduped = parlay::remove_duplicate_integers(s, 1000);

  ASSERT_EQ(deduped.size(), std::set<int>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<int>(s.begin(), s.end()), (std::set<int>(deduped.begin(), deduped.end())));
}

TEST(TestGroupBy, TestRemoveDuplicateIntegersNonContiguous) {
  auto ss = parlay::tabulate(100000, [](unsigned long long i) -> unsigned long long {
    return ((50021 * i + 61) % (1 << 20)) % 1000;
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  auto deduped = parlay::remove_duplicate_integers(s, 1000);

  ASSERT_EQ(deduped.size(), std::set<int>(s.begin(), s.end()).size());
  ASSERT_EQ(std::set<int>(s.begin(), s.end()), (std::set<int>(deduped.begin(), deduped.end())));
}

// -----------------------------------------------------------------------
//                           group_by_index
// -----------------------------------------------------------------------

TEST(TestGroupBy, TestGroupByIndex) {
  std::vector<std::pair<int, int>> a = {
      {3,35},
      {1,13},
      {3,35},
      {2,22},
      {2,29},
      {3,35},
      {2,21},
      {2,20},
      {1,19},
      {2,21},
      {1,10}
  };
  auto grouped = parlay::group_by_index(a, 4);

  ASSERT_EQ(grouped.size(), 4);

  ASSERT_EQ(grouped[0].size(), 0);
  ASSERT_EQ(grouped[1].size(), 3);
  ASSERT_EQ(grouped[2].size(), 5);
  ASSERT_EQ(grouped[3].size(), 3);
}

TEST_P(TestGroupByP, TestGroupByIndexLarge) {
  auto s = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);

  ASSERT_GE(result.size(), num_buckets);
  ASSERT_LE(result.size(), num_buckets+1);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(result[i].size(), num);
  }

  std::multiset<unsigned long long> values;
  for (const auto& vs : result) for (const auto& v : vs) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByIndexNonContiguous) {
  auto ss = parlay::tabulate(50000, [](unsigned long long i) -> unsigned long long {
    return (50021 * i + 61) % (1 << 20);
  });
  std::deque<unsigned long long> s(std::begin(ss), std::end(ss));
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);

  ASSERT_GE(result.size(), num_buckets);
  ASSERT_LE(result.size(), num_buckets+1);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x % num_buckets == bucket; });
    ASSERT_EQ(result[i].size(), num);
  }

  std::multiset<unsigned long long> values;
  for (const auto& vs : result) for (const auto& v : vs) values.insert(v);
  ASSERT_EQ(values, std::multiset<unsigned long long>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByIndexNonTrivial) {
  auto s = parlay::tabulate(20000, [](unsigned long long i) {
    return std::string(24, ' ') + std::to_string((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](const auto& x) { return std::make_tuple(std::stoull(x) % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);

  ASSERT_GE(result.size(), num_buckets);
  ASSERT_LE(result.size(), num_buckets+1);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](const auto& x) { return std::stoull(x) % num_buckets == bucket; });
    ASSERT_EQ(result[i].size(), num);
  }

  std::multiset<std::string> values;
  for (const auto& vs : result) for (const auto& v : vs) values.insert(v);
  ASSERT_EQ(values, std::multiset<std::string>(std::begin(s), std::end(s)));
}

TEST_P(TestGroupByP, TestGroupByIndexNonRelocatable) {
  auto s = parlay::tabulate(50000, [](unsigned long long i) {
    return SelfReferentialThing((50021 * i + 61) % (1 << 20));
  });
  size_t num_buckets = GetParam();
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_tuple(x.x % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);

  ASSERT_GE(result.size(), num_buckets);
  ASSERT_LE(result.size(), num_buckets+1);

  for (size_t i = 0; i < result.size(); i++) {
    size_t bucket = i;
    auto num = std::count_if(std::begin(s), std::end(s),
                             [num_buckets, bucket](auto x) { return x.x % num_buckets == bucket; });
    ASSERT_EQ(result[i].size(), num);
  }

  std::multiset<SelfReferentialThing> values;
  for (const auto& vs : result) for (const auto& v : vs) values.insert(v);
  ASSERT_EQ(values, std::multiset<SelfReferentialThing>(std::begin(s), std::end(s)));
}

// For the value-parametrized tests, we want to vary the number of groups from small to large,
// so that the buckets vary from dense to sparse
INSTANTIATE_TEST_SUITE_P(NumBuckets, TestGroupByP, testing::Values(2, 10, 100, 1000));
