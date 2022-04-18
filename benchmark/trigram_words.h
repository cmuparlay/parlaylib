// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <fstream>
#include <math.h>
#include <limits.h>
#include <parlay/primitives.h>
#include <parlay/random.h>

#define TRIGRAM_FILE PARLAY_BENCHMARK_DIRECTORY "/trigrams.txt"

struct ngram_table {
  int len;
  parlay::random r;

  struct tableEntry {
    char str[10];
    int len;
    char chars[27];
    float probs[27];
  };

  tableEntry S[27][27];

  int index(char c) {
    if (c=='_') return 26;
    else return (c-'a');
  }

  ngram_table() {
    std::ifstream ifile(TRIGRAM_FILE);
    if (!ifile.is_open()) {
      std::cout << "ngram_table: Unable to open trigram file: " << TRIGRAM_FILE << std::endl;
      abort();
    } else {
      int i=0;
      tableEntry x;
      while (ifile >> x.str >> x.len) {
        float probSum = 0.0;
        for (int j=0; j < x.len; j++) {
          float prob;
          ifile >> x.chars[j] >> prob;
          probSum += prob;
          if (j == x.len-1) x.probs[j] = 1.0;
          else x.probs[j] = probSum;
        }
        int i0 = index(x.str[0]);
        int i1 = index(x.str[1]);
        if (i0 > 26 || i1 > 26) abort();
        S[i0][i1] = x;
        i++;
      }
      len = i;
    }
  }

  using uint = unsigned int;

  char next(char c0, char c1, int i) {
    int j=0;
    tableEntry E = S[index(c0)][index(c1)];
    uint ri = (uint) r.ith_rand(i);
    double x = ((double) ri)/((double) UINT_MAX);
    while (x > E.probs[j]) j++;
    return E.chars[j];
  }

  // generates a string until the next space (space not included)
  auto word(size_t k) {
    size_t i = r.ith_rand(k);
    parlay::sequence<char> w;
    w.push_back(next('_', '_', i));
    char next_char = next('_', w[0], i + 1);
    size_t j = 1;
    while (next_char != '_') {
      w.push_back(next_char);
      j++;
      next_char = next(w[j-2], w[j-1], i + j);
    }
    return w;
  }

  // generates a string of length n, spaces included
  auto string(size_t n, size_t k) {
    size_t i = r.ith_rand(k);
    parlay::sequence<char> a(n);
    if (n == 0) return a;
    a[0] = next('_', '_', i);
    char next_char = next('_', a[0], i + 1);
    size_t j = 1;
    while (j < n) {
      a[j++] = next_char;
      next_char = next(a[j-2], a[j-1], i + j);
    }
    return a;
  }
};
