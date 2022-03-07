// Tests for incorrect usages of uninitialized memory in Parlay's algorithms
//
// How it works:
//  - If the algorithm uses uninitialized memory, it must obtain it from one of:
//      * parlay::sequence::uninitialized,
//      * parlay::internal::uninitialized_sequence,
//      * parlay::internal::uninitialized_storage
//    in order for uninitialized tracking to be correctly performed.
//  - The test case should then test the algorithm on an input sequence that
//    contains elements of type parlay::internal::UninitializedTracker
//  - If the algorithm tries to assign to or copy an uninitialized object,
//    tries to emplace an object into already initialized memory, or leaves
//    an initialized object undestroyed, the program will terminate with an
//    assertion error
//
// Note that these tests will produce false positives on some compilers because
// they rely on performing undefined behaviour and hoping that everything works
// out. This is because, in order to track whether an object is initialized or
// uninitialized, we use a special type that writes to a member flag during that
// object's destructor. If this write is actually performed and is visible, we
// can then check this flag when performing uninitialized operations to ensure
// that the memory is indeed uninitialized. Since this flag is volatile, the
// compiler should ensure that it is written to memory and visible after the
// object's destruction, but we can not guarantee this since we are doing
// undefined behaviour anyway.
//

#include "gtest/gtest.h"

#define PARLAY_DEBUG_UNINITIALIZED
#include <parlay/internal/debug_uninitialized.h>

#include <parlay/internal/bucket_sort.h>
#include <parlay/internal/counting_sort.h>
#include <parlay/internal/integer_sort.h>
#include <parlay/internal/merge_sort.h>
#include <parlay/internal/quicksort.h>
#include <parlay/internal/sample_sort.h>

#include <parlay/primitives.h>
#include <parlay/type_traits.h>

// We do not want UninitializedTracker to be memcpy relocated since we rely on its
// special member functions to keep track of whether it is initialized or not.
static_assert(!parlay::is_trivially_relocatable_v<parlay::internal::UninitializedTracker>);

TEST(TestUninitializedMemory, TestInsertionSort) {
  auto s = parlay::tabulate(10000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::insertion_sort(s.begin(), s.size(), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestUninitializedMemory, TestQuicksort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::quicksort(make_slice(s), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestUninitializedMemory, TestMergeSort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::merge_sort(make_slice(s), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_EQ(s.size(), sorted.size());
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestUninitializedMemory, TestCountSort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 10);
  });
  auto keys = parlay::internal::delayed_map(s, [](auto&& x) { return x.x; });
  auto [sorted, offset] = parlay::internal::count_sort(make_slice(s), keys, 1 << 10);
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestUninitializedMemory, TestBucketSort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::bucket_sort(make_slice(s), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestUninitializedMemory, TestSampleSort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::sample_sort(make_slice(s), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_EQ(s.size(), sorted.size());
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestUninitializedMemory, TestSampleSortInplace) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::sample_sort_inplace(make_slice(s), std::less<parlay::internal::UninitializedTracker>());
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestUninitializedMemory, TestIntegerSort) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::integer_sort(make_slice(s), [](auto s) { return s.x; });
  ASSERT_EQ(s.size(), sorted.size());
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted)));
}

TEST(TestUninitializedMemory, TestIntegerSortInPlace) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::integer_sort_inplace(make_slice(s), [](auto s) { return s.x; });
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s)));
}

TEST(TestUninitializedMemory, TestGroupByKey) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = 100;
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_pair(x.x % num_buckets, x); } );
  auto result = parlay::group_by_key(key_vals);
  ASSERT_LE(result.size(), num_buckets);
}

TEST(TestUninitializedMemory, TestGroupByKeyMove) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = 100;
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_pair(x.x % num_buckets, x); } );
  auto result = parlay::group_by_key(std::move(key_vals));
  ASSERT_LE(result.size(), num_buckets);
}

TEST(TestUninitializedMemory, TestGroupByIndex) {
  auto s = parlay::tabulate(100000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = 1000;
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_pair(x.x % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);
  ASSERT_EQ(result.size(), num_buckets);
}

TEST(TestUninitializedMemory, TestGroupByIndexSmall) {
  auto s = parlay::tabulate(10000000, [](size_t i) -> parlay::internal::UninitializedTracker {
    return (50021 * i + 61) % (1 << 20);
  });
  size_t num_buckets = 100;
  auto key_vals = parlay::delayed_map(s, [num_buckets](auto x) { return std::make_pair(x.x % num_buckets, x); } );
  auto result = parlay::group_by_index(key_vals, num_buckets);
  ASSERT_EQ(result.size(), num_buckets);
}
