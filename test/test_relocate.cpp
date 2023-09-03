#include "gtest/gtest.h"

#include <list>
#include <vector>
#include <deque>

#include <parlay/relocation.h>
#include <parlay/type_traits.h>
#include <parlay/utilities.h>

// Launder is not available in some older compilers
#ifdef __cpp_lib_launder
#define LAUNDER(x) std::launder((x))
#else
#define LAUNDER(x) (x)
#endif

// A type that is not trivially relocatable because
// it keeps a pointer to an object inside itself
struct NotTriviallyRelocatable {
  int x;
  int* px;
  explicit NotTriviallyRelocatable(int _x) : x(_x), px(&x) { }
  NotTriviallyRelocatable(const NotTriviallyRelocatable& other) : x(other.x), px(&x) { }
  NotTriviallyRelocatable(NotTriviallyRelocatable&& other) noexcept : x(other.x), px(&x) { }
};

// A type that is trivially relocatable because it is
// trivially movable and trivally destructible
struct TriviallyRelocatable {
  int x;
  explicit TriviallyRelocatable(int _x) : x(_x) { }
  TriviallyRelocatable(const TriviallyRelocatable&) = default;
  TriviallyRelocatable(TriviallyRelocatable&&) = default;
  ~TriviallyRelocatable() = default;
};

// A type that we annotate as trivially relocatable,
// even though it is not deducible as such by the compiler
struct MyTriviallyRelocatable {
  int* x;
  explicit MyTriviallyRelocatable(int _x) : x(new int(_x)) { }
  MyTriviallyRelocatable(const MyTriviallyRelocatable& other) : x(nullptr) {
    if (other.x != nullptr) {
      x = new int(*(other.x));
    }
  }
  MyTriviallyRelocatable(MyTriviallyRelocatable&& other) : x(other.x) { other.x = nullptr; }
  ~MyTriviallyRelocatable() {
    if (x != nullptr) {
      *x = -1;
      delete x;
      x = nullptr;
    }
  }
};

namespace parlay {
  
// Mark the type MyTriviallyRelocatable as trivially relocatable
template<>
struct is_trivially_relocatable<MyTriviallyRelocatable> : public std::true_type { };
  
}


static_assert(!parlay::is_trivially_relocatable_v<NotTriviallyRelocatable>);
static_assert(parlay::is_trivially_relocatable_v<TriviallyRelocatable>);
static_assert(parlay::is_trivially_relocatable_v<MyTriviallyRelocatable>);


TEST(TestRelocate, TestNotTriviallyRelocatable) {
  std::aligned_storage<sizeof(NotTriviallyRelocatable), alignof(NotTriviallyRelocatable)>::type a, b;
  NotTriviallyRelocatable* from = LAUNDER(reinterpret_cast<NotTriviallyRelocatable*>(&a));
  NotTriviallyRelocatable* to = LAUNDER(reinterpret_cast<NotTriviallyRelocatable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) NotTriviallyRelocatable(42);
  ASSERT_EQ(from->x, 42);
  ASSERT_EQ(from->px, &(from->x));
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::uninitialized_relocate(to, from);
  ASSERT_EQ(to->x, 42);
  ASSERT_EQ(to->px, &(to->x));
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~NotTriviallyRelocatable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestRelocate, TestTriviallyRelocatable) {
  std::aligned_storage<sizeof(TriviallyRelocatable), alignof(TriviallyRelocatable)>::type a, b;
  TriviallyRelocatable* from = LAUNDER(reinterpret_cast<TriviallyRelocatable*>(&a));
  TriviallyRelocatable* to = LAUNDER(reinterpret_cast<TriviallyRelocatable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) TriviallyRelocatable(42);
  ASSERT_EQ(from->x, 42);
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::uninitialized_relocate(to, from);
  ASSERT_EQ(to->x, 42);
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~TriviallyRelocatable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestRelocate, TestCustomTriviallyRelocatable) {
  std::aligned_storage<sizeof(MyTriviallyRelocatable), alignof(MyTriviallyRelocatable)>::type a, b;
  MyTriviallyRelocatable* from = LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&a));
  MyTriviallyRelocatable* to = LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) MyTriviallyRelocatable(42);
  ASSERT_EQ(*(from->x), 42);
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::uninitialized_relocate(to, from);
  ASSERT_EQ(*(to->x), 42);
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~MyTriviallyRelocatable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestRelocate, TestNotTriviallyRelocatableArray) {
  constexpr int N = 100000;
  std::vector<std::aligned_storage<sizeof(NotTriviallyRelocatable), alignof(NotTriviallyRelocatable)>::type> a(N), b(N);
  NotTriviallyRelocatable* from = LAUNDER(reinterpret_cast<NotTriviallyRelocatable*>(a.data()));
  NotTriviallyRelocatable* to = LAUNDER(reinterpret_cast<NotTriviallyRelocatable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    new (&from[i]) NotTriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(from[i].x, i);
    ASSERT_EQ(from[i].px, &(from[i].x));
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::uninitialized_relocate_n(to, from, N);
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(to[i].x, i);
    ASSERT_EQ(to[i].px, &(to[i].x));
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    to[i].~NotTriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestRelocate, TestTriviallyRelocatableArray) {
  constexpr int N = 100000;
  std::vector<std::aligned_storage<sizeof(TriviallyRelocatable), alignof(TriviallyRelocatable)>::type> a(N), b(N);
  TriviallyRelocatable* from = LAUNDER(reinterpret_cast<TriviallyRelocatable*>(a.data()));
  TriviallyRelocatable* to = LAUNDER(reinterpret_cast<TriviallyRelocatable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    new (&from[i]) TriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(from[i].x, i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::uninitialized_relocate_n(to, from, N);
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(to[i].x, i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    to[i].~TriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestRelocate, TestCustomTriviallyRelocatableArray) {
  constexpr int N = 100000;
  std::vector<std::aligned_storage<sizeof(MyTriviallyRelocatable), alignof(MyTriviallyRelocatable)>::type> a(N), b(N);
  MyTriviallyRelocatable* from = LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(a.data()));
  MyTriviallyRelocatable* to = LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    new (&from[i]) MyTriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(*(from[i].x), i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::uninitialized_relocate_n(to, from, N);
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(*(to[i].x), i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (int i = 0; i < N; i++) {
    to[i].~MyTriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestRelocate, TestRelocatableNonContiguousArray) {
  constexpr int N = 100000;
  std::deque<std::aligned_storage<sizeof(MyTriviallyRelocatable), alignof(MyTriviallyRelocatable)>::type> a(N), b(N);
  auto from = std::begin(a);
  auto to = std::begin(b);
  // -- Both from and to point to uninitialized memory

  static_assert(!parlay::is_contiguous_iterator_v<decltype(from)>);
  static_assert(!parlay::is_contiguous_iterator_v<decltype(to)>);

  auto get_from = [&](auto i) {
    return LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&from[i]));
  };

  auto get_to = [&](auto i) {
    return LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&to[i]));
  };

  for (int i = 0; i < N; i++) {
    new (get_from(i)) MyTriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(*(get_from(i)->x), i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory

  parlay::uninitialized_relocate_n(to, from, N);
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(*(get_to(i)->x), i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory

  for (int i = 0; i < N; i++) {
    get_to(i)->~MyTriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestRelocate, TestRelocatableNonRandomAccessArray) {
  constexpr int N = 1000;
  std::list<std::aligned_storage<sizeof(MyTriviallyRelocatable), alignof(MyTriviallyRelocatable)>::type> a(N), b(N);
  auto from = std::begin(a);
  auto to = std::begin(b);
  // -- Both from and to point to uninitialized memory

  static_assert(!parlay::is_random_access_iterator_v<decltype(from)>);
  static_assert(!parlay::is_random_access_iterator_v<decltype(to)>);

  auto get_from = [&](auto i) {
    auto it = from;
    std::advance(it, i);
    return LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&(*it)));
  };

  auto get_to = [&](auto i) {
    auto it = to;
    std::advance(it, i);
    return LAUNDER(reinterpret_cast<MyTriviallyRelocatable*>(&(*it)));
  };

  for (int i = 0; i < N; i++) {
    new (get_from(i)) MyTriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(*(get_from(i)->x), i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory

  parlay::uninitialized_relocate_n(to, from, N);
  for (int i = 0; i < N; i++) {
    ASSERT_EQ(*(get_to(i)->x), i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory

  for (int i = 0; i < N; i++) {
    get_to(i)->~MyTriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}

// An uninitialized memory container that holds an object of type T,
// but does not construct it or destroy it. It will move it, though.
template<typename T>
struct NonRelocatableStorage {
  char storage[sizeof(T)];
  NonRelocatableStorage() { }
  NonRelocatableStorage(NonRelocatableStorage&& other) {
    new (get_storage()) T(std::move(*other.get_storage()));
  }
  T* get_storage() {
    return LAUNDER(reinterpret_cast<T*>(storage));
  }
  ~NonRelocatableStorage() { }  // Non-trivial destructor prevents it from being trivially relocatable
};


TEST(TestRelocate, TestNonRelocatableNonRandomAccessArray) {
  constexpr int N = 1000;
  std::list<NonRelocatableStorage<NotTriviallyRelocatable>> a(N), b(N);
  auto from = std::begin(a);
  auto to = std::begin(b);

  static_assert(!parlay::is_random_access_iterator_v<decltype(from)>);
  static_assert(!parlay::is_random_access_iterator_v<decltype(to)>);
  static_assert(!parlay::is_trivially_relocatable_v<NonRelocatableStorage<NotTriviallyRelocatable>>);

  // -- Both from and to point to uninitialized memory

  auto get_from = [&](auto i) {
    auto it = from;
    std::advance(it, i);
    return it->get_storage();
  };

  auto get_to = [&](auto i) {
    auto it = to;
    std::advance(it, i);
    return it->get_storage();
  };

  for (int i = 0; i < N; i++) {
    new (get_from(i)) NotTriviallyRelocatable(i);
  }
  for (int i = 0; i < N; i++) {
    ASSERT_EQ((get_from(i)->x), i);
    ASSERT_EQ(get_from(i)->px, &(get_from(i)->x));
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory

  parlay::uninitialized_relocate_n(to, from, N);
  for (int i = 0; i < N; i++) {
    ASSERT_EQ((get_to(i)->x), i);
    ASSERT_EQ(get_to(i)->px, &(get_to(i)->x));
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory

  for (int i = 0; i < N; i++) {
    get_to(i)->~NotTriviallyRelocatable();
  }
  // -- Both from and to point to uninitialized memory
}