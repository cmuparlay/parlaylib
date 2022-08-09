#include "gtest/gtest.h"

#include <cstddef>

#include <vector>

#include <parlay/utilities.h>

using parlay::padded;

int func() { return 42; }

TEST(TestPadded, TestTypeApplicability) {

  enum E { Ea, Eb };
  enum class EC { a, b };

  struct F {
    void f() {}
    int x;
  };

  int x = 1;
  
  padded<int> i = 1;
  ASSERT_EQ(i, 1);
  padded<double> f = 1.0;
  ASSERT_NEAR(f, 1.0, 1e-10);
  padded<std::vector<int>> v = std::vector<int>{1,2,3};
  ASSERT_EQ(v, (std::vector<int>{1,2,3}));
  padded<int*> ia = &x;
  ASSERT_EQ(ia, &x);
  padded<std::nullptr_t> np = nullptr;
  // For some reason, implicit conversion to std::nullptr_t
  // doesn't work in operator== on MSVC, or GCC 10 and below?
  ASSERT_EQ(std::nullptr_t{np}, nullptr);
  padded<E> e = Ea;
  ASSERT_EQ(e, Ea);
  padded<EC> ec = EC::a;
  ASSERT_EQ(ec, EC::a);
  padded<void (F::*)()> mfp = &F::f;
  ASSERT_EQ(mfp, &F::f);
  padded<int (F::*)> mop = &F::x;
  ASSERT_EQ(mop, &F::x);
  padded<int(*)()> fp = &func;
  ASSERT_EQ(fp, &func);
}

TEST(TestPadded, TestPointerDeref) {
  int x = 1;
  padded<int*> pp = &x;
  ASSERT_EQ(*pp, 1);
  *pp = 2;
  ASSERT_EQ(x, 2);
  x = 3;
  ASSERT_EQ(*pp, 3);
}

TEST(TestPadded, TestFunctionPtr) {
  padded<int(*)()> fp = &func;
  ASSERT_EQ(fp, &func);
  auto x = fp();
  ASSERT_EQ(x, 42);

  const padded<int(*)()> cfp = &func;
  ASSERT_EQ(cfp, &func);
  auto y = cfp();
  ASSERT_EQ(y, 42);
}

TEST(TestPadded, TestMemberFunctionPtr) {
  struct F {
    int f(int y) { return y + x; }
    int f(int y) const { return y + x + 1; }
    int x;
    F(int x_) : x(x_) {}
  };

  // Unfortunately I can't figure out how to
  // make padded member-function pointers directly
  // invocable, but that's fine because only
  // crazy people use member-function pointers, right?
  //
  // So notice that we have to actually access the
  // underlying object here with .x

  padded<int (F::*)(int)> pf = &F::f;
  F obj(10);
  auto x = (obj.*(pf.x))(42);       // <---- access the pointer directly with .x
  ASSERT_EQ(x, 52);

  padded<int (F::*)(int) const> cpf = &F::f;
  const F cobj(20);
  auto y = (cobj.*(cpf.x))(80);    // <---- access the pointer directly with .x
  ASSERT_EQ(y, 101);
}

TEST(TestPadded, TestMemberObjectPointer) {
  struct F {
    int x;
    F(int x_) : x(x_) {}
  };

  // Just like member function pointers, I don't know how
  // to make them work directly, so .x is needed to access
  // the underlying object

  padded<int (F::*)> mop = &F::x;
  F obj(25);
  auto x = (obj.*(mop.x));          // <---- access the pointer directly with .x
  ASSERT_EQ(x, 25);

  padded<int (F::*)> cmop = &F::x;
  F cobj(111);
  auto y = (cobj.*(cmop.x));        // <---- access the pointer directly with .x
  ASSERT_EQ(y, 111);
}

TEST(TestPadded, TestScalarInitialization) {
  padded<int> default_i;
  ASSERT_EQ(default_i, 0);
  padded<int> value_i{};
  ASSERT_EQ(value_i, 0);
  padded<int> explicit_i(1);
  ASSERT_EQ(explicit_i, 1);
  padded<int> explicit_brace_i{2};
  ASSERT_EQ(explicit_brace_i, 2);
  padded<int> copy_initialize = 3;
  ASSERT_EQ(copy_initialize, 3);
}

TEST(TestPadded, TestScalarAssignment) {
  padded<int> i;
  ASSERT_EQ(i, 0);
  i = 1;
  ASSERT_EQ(i, 1);
  int x = 2;
  i = x;
  ASSERT_EQ(i, 2);
  x = 3;
  i = std::move(x);       // NOLINT
  ASSERT_EQ(i, 3);

  i = padded<int>{4};
  ASSERT_EQ(i, 4);
  padded<int> i2{5};
  i = i2;
  ASSERT_EQ(i, 5);
  i2 = 6;
  i = std::move(i2);      // NOLINT
  ASSERT_EQ(i, 6);
}

TEST(TestPadded, TestScalarCompositeAssignment) {
  padded<int> i{1};
  ASSERT_EQ(i, 1);
  i += 2;
  ASSERT_EQ(i, 3);
}

TEST(TestPadded, TestScalarLocalRefBinding) {
  padded<int> i{1};
  ASSERT_EQ(i, 1);
  int iv = i;
  ASSERT_EQ(iv, 1);
  const int& cir = i;
  ASSERT_EQ(cir, 1);
  int& ir = i;
  ASSERT_EQ(iv, 1);
  ir = 2;
  ASSERT_EQ(i, 2);
  ASSERT_EQ(cir, 2);
  i = 3;
  int&& irr = std::move(i);           // NOLINT
  ASSERT_EQ(irr, 3);
  i = 4;
  const auto& ci = i;
  const int&& cirr = std::move(ci);   // NOLINT
  ASSERT_EQ(cirr, 4);
}

TEST(TestPadded, TestScalarParameterBinding) {
  padded<int> i = 42;
  [&](int x) { ASSERT_EQ(x, 42); }(i);
  [&](int& x) { ASSERT_EQ(x, 42); }(i);
  [&](int& x) { x++; }(i);
  ASSERT_EQ(i, 43);
  [&](const int& x) { ASSERT_EQ(x, 43); }(i);
  [&](int&& x) { ASSERT_EQ(x, 43); }(std::move(i));           // NOLINT
  [&](const int&& x) { ASSERT_EQ(x, 43); }(std::move(i));     // NOLINT

  const auto& ci = i;
  [&](int x) { ASSERT_EQ(x, 43); }(ci);
  [&](const int& x) { ASSERT_EQ(x, 43); }(ci);
  [&](const int&& x) { ASSERT_EQ(x, 43); }(std::move(ci));    // NOLINT
}

TEST(TestPadded, TestClassMethods) {
  struct S { int f() const { return 42; } };

  padded<S> p;
  int x = p.f();
  ASSERT_EQ(x, 42);
}

TEST(TestPadded, TestClassInitialization) {
  padded<std::vector<int>> default_v;
  ASSERT_TRUE(default_v.empty());
  padded<std::vector<int>> value_v{};
  ASSERT_TRUE(value_v.empty());
  padded<std::vector<int>> explicit_v(1,1);
  ASSERT_EQ(explicit_v, (std::vector<int>(1,1)));
  padded<std::vector<int>> init_list_v{1,2,3,4,5};
  ASSERT_EQ(init_list_v, (std::vector<int>{1,2,3,4,5}));

  std::vector<int> v(10,10);
  padded<std::vector<int>> init_with_copy(v);
  ASSERT_EQ(init_with_copy, v);
  padded<std::vector<int>> init_with_move(std::move(v));
  ASSERT_EQ(init_with_move, (std::vector<int>(10,10)));
  ASSERT_TRUE(v.empty());
  padded<std::vector<int>> init_with_temp(std::vector<int>(20,20));
  ASSERT_EQ(init_with_temp, (std::vector<int>(20,20)));

  v = std::vector<int>(30,30);
  padded<std::vector<int>> copy_init_v_with_copy = v;
  ASSERT_EQ(copy_init_v_with_copy, v);
  padded<std::vector<int>> copy_init_v_with_move = std::move(v);
  ASSERT_EQ(copy_init_v_with_move, (std::vector<int>(30,30)));
  ASSERT_TRUE(v.empty());
  padded<std::vector<int>> copy_init_v_with_temp = std::vector<int>(5,2);
  ASSERT_EQ(copy_init_v_with_temp, (std::vector<int>(5,2)));
}

TEST(TestPadded, TestClassAssignment) {
  padded<std::vector<int>> pv;

  std::vector<int> v(10,10);
  pv = v;                                         // copy assign
  ASSERT_EQ(pv, v);
  pv = std::move(v);                              // move assign
  ASSERT_EQ(pv, (std::vector<int>(10,10)));
  ASSERT_TRUE(v.empty());
  pv = std::vector<int>(20,20);                   // move assign temp
  ASSERT_EQ(pv, (std::vector<int>(20,20)));
}

TEST(TestPadded, TestClassLocalRefBinding) {
  padded<std::vector<int>> v{1,2,3};
  ASSERT_EQ(v, (std::vector<int>{1,2,3}));
  std::vector<int> vv = v;
  ASSERT_EQ(v, vv);
  const std::vector<int>& cvr = v;
  ASSERT_EQ(cvr, vv);
  std::vector<int>& vr = v;
  ASSERT_EQ(vr, v);
  vr.push_back(4);
  ASSERT_EQ(v, (std::vector<int>{1,2,3,4}));
  ASSERT_EQ(cvr, (std::vector<int>{1,2,3,4}));
  v.push_back(5);
  std::vector<int>&& vrr = std::move(v);
  ASSERT_EQ(vrr, (std::vector<int>{1,2,3,4,5}));
  v.push_back(6);
  const auto& cv = v;
  const std::vector<int>&& cvrr = std::move(cv);
  ASSERT_EQ(cvrr, (std::vector<int>{1,2,3,4,5,6}));
}

TEST(TestPadded, TestClassParameterBinding) {
  padded<std::vector<int>> pv{1,2,3};
  std::vector<int> v = pv;
  [&](std::vector<int> x) { ASSERT_EQ(x, v); }(pv);
  [&](std::vector<int>& x) { ASSERT_EQ(x, v); }(pv);
  [&](std::vector<int>& x) { x.push_back(4); }(pv);
  v.push_back(4);
  ASSERT_EQ(pv, v);
  [&](const std::vector<int>& x) { ASSERT_EQ(x, v); }(pv);
  [&](std::vector<int>&& x) { ASSERT_EQ(x, v); }(std::move(pv));
  [&](std::vector<int>&& x) { std::vector<int> y(std::move(x)); ASSERT_EQ(y, v); }(std::move(pv));
  ASSERT_TRUE(pv.empty());
  pv = std::vector<int>{1,2,3,4,5};
  v.push_back(5);
  [&](const std::vector<int>&& x) { ASSERT_EQ(x, v); }(std::move(pv));

  const auto& cpv = pv;
  [&](std::vector<int> x) { ASSERT_EQ(x, v); }(cpv);
  [&](const std::vector<int>& x) { ASSERT_EQ(x, v); }(cpv);
  [&](const std::vector<int>&& x) { ASSERT_EQ(x, v); }(std::move(cpv));
}

TEST(TestPadded, TestClassOperatorOverloads) {
  struct S {
    int operator*() { return 42; }
    int operator*() const { return 43; }

    int operator[](int i) { return i; }
    int operator[](int i) const { return i + 1; }

    int operator()(int i, int j) { return i + j; }
    int operator()(int i, int j) const { return i + j + 1; }

    int operator+(double) { return 84; }
    int operator+(double) const { return 85; }

    int operator+=(int i) { return i; }
    int operator+=(int i) const { return i + 1; }
  };

  struct S2 {
    int operator+(S&) { return 100; }
    int operator+(const S&) { return 101; }
  };

  padded<S> p;
  const auto& cp = p;

  {
    int x = *p;
    ASSERT_EQ(x, 42);
    int y = *cp;
    ASSERT_EQ(y, 43);
  }

  {
    int x = p[42];
    ASSERT_EQ(x, 42);
    int y = cp[42];
    ASSERT_EQ(y, 43);
  }

  {
    int x = p(42, 5);
    ASSERT_EQ(x, 47);
    int y = cp(42, 10);
    ASSERT_EQ(y, 53);
  }

  {
    int x = p + 1.0;
    ASSERT_EQ(x, 84);
    int y = cp + 1.0;
    ASSERT_EQ(y, 85);
  }

  {
    S2 s2;
    int x = s2 + p;
    ASSERT_EQ(x, 100);
    int y = s2 + cp;
    ASSERT_EQ(y, 101);
  }

  {
    int x = p += 1;
    ASSERT_EQ(x, 1);
    int y = cp += 2;
    ASSERT_EQ(y, 3);
  }
}

TEST(TestPadded, TestScalarConst) {
  padded<const int> p{5};
  ASSERT_EQ(p, 5);
}

TEST(TestPadded, TestNonDefaultConstructibleClass) {
  struct X {
    X() = delete;
    explicit X(int x_) : x(x_) { }
    int x;
  };
  padded<X> x(5);
}
