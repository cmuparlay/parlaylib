#include "gtest/gtest.h"

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

// This really just checks that it compiles
// since we do not test any outcomes
TEST(TestRandom, TestRandom) {
  parlay::random r;
  for (size_t i = 0; i < 100000; i++) {
    r.ith_rand(i);
  }
  r = r.next();
  for (size_t i = 0; i < 100000; i++) {
    r.ith_rand(i);
  }
}

TEST(TestRandom, TestRandomShuffle) {
  auto s = parlay::tabulate(100000, [](long long i) -> long long {
    return (50021 * i + 61) % (1 << 20);
  });
  auto s2 = parlay::random_shuffle(s);
  std::sort(s.begin(), s.end());
  std::sort(s2.begin(), s2.end());
  ASSERT_EQ(s, s2);
}

TEST(TestRandom, TestRandomPermutation) {
  auto p = parlay::random_permutation<int>(100000);
  auto s = parlay::tabulate(100000, [](int i) -> int {
    return i;
  });
  std::sort(p.begin(), p.end());
  ASSERT_EQ(p, s);
}
