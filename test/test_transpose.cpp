#include <parlay/sequence.h>

#include <parlay/internal/transpose.h>

#include "gtest/gtest.h"

TEST(TestTranspose, TestTransposeSmall) {
  constexpr size_t n = 3, m = 3;
  auto seq = parlay::sequence<int>{1,2,3,4,5,6,7,8,9};
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>{1,4,7,2,5,8,3,6,9};
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose<parlay::copy_assign_tag, decltype(seq.begin()), decltype(seq.begin())>
      T(seq.begin(), out.begin());
  T.trans(n, m);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeLarge) {
  constexpr size_t n = 1000, m = 1000;
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) { return n*(i%m) + i/m; });
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose<parlay::copy_assign_tag, decltype(seq.begin()), decltype(seq.begin())>
      T(seq.begin(), out.begin());
  T.trans(n, m);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeRow) {
  constexpr size_t n = 10000, m = 1;
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  parlay::internal::transpose<parlay::copy_assign_tag, decltype(seq.begin()), decltype(seq.begin())>
      T(seq.begin(), out.begin());
  T.trans(n, m);
  // Transposing a row does not change the row-major representation
  ASSERT_EQ(seq, out);
}

TEST(TestTranspose, TestTransposeCol) {
  constexpr size_t n = 1, m = 10000;
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  parlay::internal::transpose<parlay::copy_assign_tag, decltype(seq.begin()), decltype(seq.begin())>
      T(seq.begin(), out.begin());
  T.trans(n, m);
  // Transposing a column does not change the row-major representation
  ASSERT_EQ(seq, out);
}

template<typename T>
void print(const T& A, size_t n, size_t m) {
  std::cout << std::string(20, '=') << '\n';
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < m; j++) {
      std::cout << A[i*m+j] << " ";
    }
    std::cout << std::endl;
  }
}

TEST(TestTranspose, TestBlockTransposeSmall) {
  constexpr size_t n = 3, m = 9, n_blocks = 3, n_buckets = 3, chunk_size = 3;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto inOffsets = parlay::sequence<int>::from_function(n_blocks*n_buckets+1, [&](int i) { return i*chunk_size; });
  auto outOffsets = inOffsets;
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::blockTrans<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), decltype(inOffsets.begin()), decltype(outOffsets.begin())>
      T(seq.begin(), out.begin(), inOffsets.begin(), outOffsets.begin());
  T.trans(n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestBlockTransposeSmall2) {
  constexpr size_t n = 3, m = 8, n_blocks = 3, n_buckets = 2, chunk_size = 4;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto inOffsets = parlay::sequence<int>::from_function(n_blocks*n_buckets+1, [&](int i) { return i*chunk_size; });
  auto outOffsets = inOffsets;
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::blockTrans<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), decltype(inOffsets.begin()), decltype(outOffsets.begin())>
      T(seq.begin(), out.begin(), inOffsets.begin(), outOffsets.begin());
  T.trans(n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestBlockTransposeLarge) {
  constexpr size_t n = 3000, m = 3000, n_blocks = 3000, n_buckets = 100, chunk_size = 30;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto inOffsets = parlay::sequence<int>::from_function(n_blocks*n_buckets+1, [&](int i) { return i*chunk_size; });
  auto outOffsets = inOffsets;
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::blockTrans<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), decltype(inOffsets.begin()), decltype(outOffsets.begin())>
      T(seq.begin(), out.begin(), inOffsets.begin(), outOffsets.begin());
  T.trans(n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestBlockTransposeRow) {
  constexpr size_t n = 1, m = 10000, n_blocks = 1, n_buckets = 100, chunk_size = 100;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto inOffsets = parlay::sequence<int>::from_function(n_blocks*n_buckets+1, [&](int i) { return i*chunk_size; });
  auto outOffsets = inOffsets;
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::blockTrans<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), decltype(inOffsets.begin()), decltype(outOffsets.begin())>
      T(seq.begin(), out.begin(), inOffsets.begin(), outOffsets.begin());
  T.trans(n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestBlockTransposeCol) {
  constexpr size_t n = 10000, m = 1, n_blocks = 10000, n_buckets = 1, chunk_size = 1;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto inOffsets = parlay::sequence<int>::from_function(n_blocks*n_buckets+1, [&](int i) { return i*chunk_size; });
  auto outOffsets = inOffsets;
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::blockTrans<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), decltype(inOffsets.begin()), decltype(outOffsets.begin())>
      T(seq.begin(), out.begin(), inOffsets.begin(), outOffsets.begin());
  T.trans(n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeBucketsSmall) {
  constexpr size_t n = 4, m = 8, n_blocks = 4, n_buckets = 4, chunk_size = 2;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto counts = parlay::sequence<size_t>::from_function(n_blocks*n_buckets, [&](int) { return chunk_size; });
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose_buckets<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), size_t>
      (seq.begin(), out.begin(), counts, n*m, m, n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeBucketsLarge) {
  constexpr size_t n = 4096, m = 1024, n_blocks = 1024, n_buckets = 1024, chunk_size = 4;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto counts = parlay::sequence<size_t>::from_function(n_blocks*n_buckets, [&](int) { return chunk_size; });
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose_buckets<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), size_t>
      (seq.begin(), out.begin(), counts, n*m, m, n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeBucketsRow) {
  constexpr size_t n = 1, m = 1 << 23, n_blocks = 1, n_buckets = 1 << 20, chunk_size = 1 << 3;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto counts = parlay::sequence<size_t>::from_function(n_blocks*n_buckets, [&](int) { return chunk_size; });
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose_buckets<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), size_t>
      (seq.begin(), out.begin(), counts, n*m, m, n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}

TEST(TestTranspose, TestTransposeBucketsCol) {
  constexpr size_t n = 1 << 23, m = 1, n_blocks = 1 << 23, n_buckets = 1, chunk_size = 1;
  ASSERT_EQ(n_blocks * n_buckets * chunk_size, n * m);
  auto seq = parlay::sequence<int>::from_function(n*m, [&](int i) { return i; });
  auto out = parlay::sequence<int>(seq.size());
  auto ans = parlay::sequence<int>::from_function(n*m, [&](int i) {
    return i%chunk_size + (n_buckets*chunk_size)*(i%(chunk_size*n_blocks)/chunk_size) + chunk_size*(i/(chunk_size*n_blocks));
  });
  auto counts = parlay::sequence<size_t>::from_function(n_blocks*n_buckets, [&](int) { return chunk_size; });
  ASSERT_EQ(seq.size(), n*m);
  ASSERT_EQ(out.size(), n*m);
  ASSERT_EQ(ans.size(), n*m);
  parlay::internal::transpose_buckets<parlay::copy_assign_tag, decltype(seq.begin()), decltype(out.begin()), size_t>
      (seq.begin(), out.begin(), counts, n*m, m, n_blocks, n_buckets);
  ASSERT_EQ(out, ans);
}
