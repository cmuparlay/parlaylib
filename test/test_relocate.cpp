#include "gtest/gtest.h"

#include <list>
#include <vector>
#include <deque>

#include <parlay/range.h>
#include <parlay/relocation.h>
#include <parlay/type_traits.h>
#include <parlay/utilities.h>

#include <parlay/internal/uninitialized_iterator.h>


// A type that is not trivially relocatable because
// it keeps a pointer to an object inside itself
struct NotTriviallyRelocatable {
  int x;
  int* px;
  explicit NotTriviallyRelocatable(int _x) : x(_x), px(&x) { }
  NotTriviallyRelocatable(const NotTriviallyRelocatable& other) : x(other.x), px(&x) { }
  NotTriviallyRelocatable(NotTriviallyRelocatable&& other) noexcept : x(other.x), px(&x) { }
  int get() const { assert(px == &x); return *px; }
};

// A type that is trivially relocatable because it is
// trivially movable and trivially destructible
struct TriviallyRelocatable {
  int x;
  explicit TriviallyRelocatable(int _x) : x(_x) { }
  TriviallyRelocatable(const TriviallyRelocatable&) = default;
  TriviallyRelocatable(TriviallyRelocatable&&) = default;
  ~TriviallyRelocatable() = default;
  int get() const { return x; }
};

// A type that we annotate as trivially relocatable,
// even though it is not deducible as such by the compiler
struct PARLAY_TRIVIALLY_RELOCATABLE MyTriviallyRelocatable {
  int* x;
  explicit MyTriviallyRelocatable(int _x) : x(new int(_x)) { }
  MyTriviallyRelocatable(const MyTriviallyRelocatable& other) : x(nullptr) {
    if (other.x != nullptr) {
      x = new int(*(other.x));
    }
  }
  MyTriviallyRelocatable(MyTriviallyRelocatable&& other) noexcept : x(other.x) { other.x = nullptr; }
  ~MyTriviallyRelocatable() {
    if (x != nullptr) {
      *x = -1;
      delete x;
      x = nullptr;
    }
  }
  int get() const { return *x; }
};

#if defined(PARLAY_MUST_SPECIALIZE_IS_TRIVIALLY_RELOCATABLE)
namespace parlay {
  
// Mark the type MyTriviallyRelocatable as trivially relocatable
template<>
PARLAY_ASSUME_TRIVIALLY_RELOCATABLE(MyTriviallyRelocatable);
  
}
#endif


static_assert(!parlay::is_trivially_relocatable_v<NotTriviallyRelocatable>);
static_assert(parlay::is_trivially_relocatable_v<TriviallyRelocatable>);
static_assert(parlay::is_trivially_relocatable_v<MyTriviallyRelocatable>);


TEST(TestRelocateAt, TestNotTriviallyRelocatable) {
  parlay::uninitialized<NotTriviallyRelocatable> a, b;
  NotTriviallyRelocatable* source = &a.value;
  NotTriviallyRelocatable* dest = &b.value;
  // -- Both source and dest point to uninitialized memory
  
  ::new (source) NotTriviallyRelocatable(42);
  ASSERT_EQ(source->x, 42);
  ASSERT_EQ(source->px, &(source->x));
  // -- Now source points to a valid object, and dest points to uninitialized memory

  parlay::relocate_at(source, dest);
  ASSERT_EQ(dest->x, 42);
  ASSERT_EQ(dest->px, &(dest->x));
  // -- Now dest points to a valid object, and source points to uninitialized memory

  std::destroy_at(dest);
  // -- Both source and dest point to uninitialized memory
}


TEST(TestRelocateAt, TestTriviallyRelocatable) {
  parlay::uninitialized<TriviallyRelocatable> a, b;
  TriviallyRelocatable* source = &a.value;
  TriviallyRelocatable* dest = &b.value;
  // -- Both source and dest point to uninitialized memory
  
  ::new (source) TriviallyRelocatable(42);
  ASSERT_EQ(source->x, 42);
  // -- Now source points to a valid object, and dest points to uninitialized memory

  parlay::relocate_at(source, dest);
  ASSERT_EQ(dest->x, 42);
  // -- Now dest points to a valid object, and source points to uninitialized memory

  std::destroy_at(dest);
  // -- Both source and dest point to uninitialized memory
}


TEST(TestRelocateAt, TestCustomTriviallyRelocatable) {
  parlay::uninitialized<MyTriviallyRelocatable> a, b;
  MyTriviallyRelocatable* source = &a.value;
  MyTriviallyRelocatable* dest = &b.value;
  // -- Both source and dest point to uninitialized memory

  ::new (source) MyTriviallyRelocatable(42);
  ASSERT_EQ(*(source->x), 42);
  // -- Now source points to a valid object, and dest points to uninitialized memory

  parlay::relocate_at(source, dest);
  ASSERT_EQ(*(dest->x), 42);
  // -- Now dest points to a valid object, and source points to uninitialized memory
  
  std::destroy_at(dest);
  // -- Both source and dest point to uninitialized memory
}


TEST(TestRelocate, TestRelocate) {
  alignas(std::unique_ptr<int>) char storage[sizeof(std::unique_ptr<int>)];
  auto* up = ::new (&storage) std::unique_ptr<int>{std::make_unique<int>(42)};

  auto x = parlay::relocate(up);
  ASSERT_EQ(*x, 42);
}

template <typename TestParams>
class TestRangeRelocate : public testing::Test { };

template<typename Container, typename T, bool UseIterator>
struct RangeRelocateTestParams {
  using container = Container;
  using value_type = T;
  static constexpr inline bool use_iterator = UseIterator;
};

// Test on the cartesian product of <Vector, Deque, List> x
//                                  <TriviallyRelocatable, NotTriviallyRelocatable, MyTriviallyRelocatable> x
//                                  <begin/end version, _n version>
using TestTypes = ::testing::Types<
    RangeRelocateTestParams<std::vector<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, true>,
    RangeRelocateTestParams<std::vector<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::vector<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::vector<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, false>,
    RangeRelocateTestParams<std::vector<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, false>,
    RangeRelocateTestParams<std::vector<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, false>,

    RangeRelocateTestParams<std::deque<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, true>,
    RangeRelocateTestParams<std::deque<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::deque<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::deque<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, false>,
    RangeRelocateTestParams<std::deque<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, false>,
    RangeRelocateTestParams<std::deque<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, false>,

    RangeRelocateTestParams<std::list<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, true>,
    RangeRelocateTestParams<std::list<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::list<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, true>,
    RangeRelocateTestParams<std::list<parlay::uninitialized<TriviallyRelocatable>>, TriviallyRelocatable, false>,
    RangeRelocateTestParams<std::list<parlay::uninitialized<NotTriviallyRelocatable>>, NotTriviallyRelocatable, false>,
    RangeRelocateTestParams<std::list<parlay::uninitialized<MyTriviallyRelocatable>>, MyTriviallyRelocatable, false>
>;

TYPED_TEST_SUITE(TestRangeRelocate, TestTypes);

TYPED_TEST(TestRangeRelocate, TestTriviallyRelocatable) {
  constexpr int N = 100000;
  typename TypeParam::container source(N), dest(N);

  using T = typename TypeParam::value_type;
  static_assert(std::is_same_v<typename TypeParam::container::value_type, parlay::uninitialized<T>>);

  // Initialize elements of source
  auto s_it = source.begin();
  for (int i = 0; i < N; i++, ++s_it) {
    ::new (&(s_it->value)) T(i);
    ASSERT_EQ(s_it->value.get(), i);
  }

  // -- Now source points to a range of valid objects, and dest points to a range of uninitialized objects

  auto source_begin = parlay::internal::uninitialized_iterator_adaptor{source.begin()};
  auto source_end = parlay::internal::uninitialized_iterator_adaptor{source.end()};
  auto dest_begin = parlay::internal::uninitialized_iterator_adaptor{dest.begin()};
  auto dest_end = parlay::internal::uninitialized_iterator_adaptor{dest.end()};

  if constexpr (TypeParam::use_iterator) {
    auto result = parlay::uninitialized_relocate(source_begin, source_end, dest_begin);
    ASSERT_EQ(result, dest_end);
  }
  else {
    auto [s_result, d_result] = parlay::uninitialized_relocate_n(source_begin, N, dest_begin);
    ASSERT_EQ(s_result, source_end);
    ASSERT_EQ(d_result, dest_end);
  }

  // -- Now dest points to a range of valid objects, and source points to a range of uninitialized objects

  auto d_it = dest.begin();
  for (int i = 0; i < N; i++, ++d_it) {
    ASSERT_EQ(d_it->value.get(), i);
    std::destroy_at(&(d_it->value));
  }

  // -- Both source and dest point to uninitialized memory
}
