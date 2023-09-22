#include "gtest/gtest.h"

#include <utility>

#include <parlay/internal/concurrency/big_atomic.h>

struct ManyLongs {
    long long x, y, z;
};

static_assert(sizeof(ManyLongs) == 24);
static_assert(std::is_trivially_copyable_v<ManyLongs>);

TEST(TestBigAtomic, TestDefaultContruct) {
    parlay::big_atomic<ManyLongs> ba;
}
