#include "gtest/gtest.h"

#include <string>

#include <parlay/delayed_sequence.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <parlay/delayed.h>

#include "range_utils.h"

// ---------------------------------------------------------------------------------------
//                                     RAD VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedZip, TestRadZipSimple) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  parlay::sequence<int> b = {2,3,4,5,6,7,8,9,10,11};
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x+1);
  }
}

TEST(TestDelayedZip, TestRadZipDiffSize) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  parlay::sequence<int> b = {2,3,4,5,6};

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), b.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x+1);
  }
}

TEST(TestDelayedZip, TestRadZipMutable) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  parlay::sequence<int> b = {2,3,4,5,6,7,8,9,10,11};
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    x--;
    y++;
  }

  for (size_t i = 0; i < a.size(); i++) {
    ASSERT_EQ(a[i] + 3, b[i]);
  }
}

TEST(TestDelayedZip, TestRadZipStrings) {
  parlay::sequence<int> a = {1,2,3,4,5,6,7,8,9,10};
  auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });  // Pad to make strings not SSO
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}

TEST(TestDelayedZip, TestRadZipCopyByValue) {
  parlay::sequence<std::tuple<int,std::string>> res;

  // After this block, b goes out of scope, so if we accidentally
  // keep references instead of making copies then bad things
  // will happen!
  {
    parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });
    res = parlay::to_sequence(parlay::delayed::zip(a,b));
  }

  for (auto [x,y] : res) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}

TEST(TestDelayedZip, TestRadZipUncopyable) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}

TEST(TestDelayedZip, TestRadZipWithDelayed) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, y);
  }
}

TEST(TestDelayedZip, TestRadZipWithTemporaryRange) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  auto zipped = parlay::delayed::zip(a, parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; }));
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, y);
  }
}

TEST(TestDelayedZip, TestRadZipWithDelayedUncopyable) {
  parlay::sequence<int> a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto b = parlay::delayed_map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}

TEST(TestDelayedZip, TestRadZipNoConst) {
  parlay::NonConstRange a{10};
  parlay::NonConstRange b{10};
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x);
  }
}

// ---------------------------------------------------------------------------------------
//                                     BID VERSION
// ---------------------------------------------------------------------------------------

TEST(TestDelayedZip, TestBidZipSimple) {
  auto a = parlay::block_iterable_wrapper(parlay::tabulate(50000, [](size_t i) { return i+1; }));
  auto b = parlay::block_iterable_wrapper(parlay::tabulate(50000, [](size_t i) { return i+2; }));
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x+1);
  }
}


TEST(TestDelayedZip, TestBidZipDiffSize) {
  auto a = parlay::block_iterable_wrapper(parlay::to_sequence(parlay::iota(100000)));
  auto b = parlay::to_sequence(parlay::iota(50000));
  auto c = parlay::to_sequence(parlay::iota(25000));

  auto zipped = parlay::delayed::zip(a,b,c);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), c.size());

  for (auto [x,y,z] : zipped) {
    ASSERT_EQ(y, x);
    ASSERT_EQ(y, z);
  }
}

TEST(TestDelayedZip, TestBidZipMutable) {
  auto a = parlay::to_sequence(parlay::iota<int>(50000));
  auto b = parlay::to_sequence(parlay::iota<int>(50000));
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(parlay::block_iterable_wrapper(a),b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    x--;
    y++;
  }

  for (size_t i = 0; i < a.size(); i++) {
    ASSERT_EQ(a[i] + 2, b[i]);
  }
}

TEST(TestDelayedZip, TestBidZipStrings) {
  auto a = parlay::to_sequence(parlay::iota<int>(10000));
  auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });  // Pad to make strings not SSO
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(parlay::block_iterable_wrapper(a),b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}


TEST(TestDelayedZip, TestBidZipToSeq) {
  auto a = parlay::block_iterable_wrapper(parlay::tabulate(50000, [](size_t i) { return i+1; }));
  auto b = parlay::block_iterable_wrapper(parlay::tabulate(50000, [](size_t i) { return i+2; }));
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  auto s = parlay::delayed::to_sequence(zipped);
  ASSERT_EQ(s.size(), zipped.size());

  for (size_t i = 0; i < s.size(); i++) {
    ASSERT_EQ(s[i], std::make_tuple(i+1, i+2));
  }
}

TEST(TestDelayedZip, TestBidZipCopyByValue) {
  parlay::sequence<std::tuple<int,std::string>> res;

  // After this block, b goes out of scope, so if we accidentally
  // keep references instead of making copies then bad things
  // will happen!
  {
    auto a = parlay::to_sequence(parlay::iota<int>(10000));
    auto b = parlay::map(a, [](auto x) { return std::string(50, ' ') + std::to_string(x); });
    auto zipped = parlay::delayed::zip(parlay::block_iterable_wrapper(a),b);
    static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
    res = parlay::delayed::to_sequence(zipped);
  }

  for (auto [x,y] : res) {
    ASSERT_EQ(std::string(50, ' ') + std::to_string(x), y);
    ASSERT_EQ(x, std::stoi(y));
  }
}

TEST(TestDelayedZip, TestBidZipUncopyable) {
  auto a = parlay::to_sequence(parlay::iota<int>(10000));
  auto b = parlay::map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(parlay::block_iterable_wrapper(a),b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}

TEST(TestDelayedZip, TestBidZipWithDelayed) {
  auto a = parlay::block_iterable_wrapper(parlay::to_sequence(parlay::iota<int>(10000)));
  auto b = parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x+1, y);
  }
}

TEST(TestDelayedZip, TestBidZipWithTemporaryRange) {
  auto a = parlay::block_iterable_wrapper(parlay::to_sequence(parlay::iota<int>(10000)));

  auto zipped = parlay::delayed::zip(a, parlay::delayed_tabulate(a.size(), [](size_t x) -> int { return x + 1; }));
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x+1, y);
  }
}

TEST(TestDelayedZip, TestBidZipWithDelayedUncopyable) {
  auto a = parlay::to_sequence(parlay::iota<int>(10000));
  auto b = parlay::delayed_map(a, [](int x) { return std::make_unique<int>(x); });
  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(parlay::block_iterable_wrapper(a),b);
  static_assert(!parlay::is_random_access_range_v<decltype(zipped)>);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(x, *y);
  }
}

TEST(TestDelayedZip, TestBidZipNoConst) {
  auto a = parlay::block_iterable_wrapper(parlay::NonConstRange{10});
  auto b = parlay::block_iterable_wrapper(parlay::NonConstRange{10});

  ASSERT_EQ(a.size(), b.size());

  auto zipped = parlay::delayed::zip(a,b);
  ASSERT_EQ(zipped.size(), a.size());

  for (auto [x,y] : zipped) {
    ASSERT_EQ(y, x);
  }
}

TEST(TestDelayedZip, TestEnumerate) {
  auto s = parlay::delayed_map(parlay::iota<int>(10000), [](int x) { return std::make_unique<int>(x); });

  for (auto [i, x] : parlay::delayed::enumerate(s)) {
    ASSERT_EQ(i, *x);
  }
}

TEST(TestDelayedZip, TestZipWith) {
  auto s = parlay::delayed_map(parlay::iota<int>(10000), [](int x) { return std::make_unique<int>(x); });
  auto zw = parlay::delayed::zip_with([](int x, auto&& up) {
    return x + *up;
  }, parlay::iota<int>(10000), s);

  ASSERT_EQ(zw.size(), 10000);

  auto it = zw.begin();
  for (size_t i = 0; i < zw.size(); i++) {
    ASSERT_EQ(2*i, *it);
    it++;
  }
  ASSERT_EQ(it, zw.end());
}
