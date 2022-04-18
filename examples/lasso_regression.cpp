#include <stdlib.h>
#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/io.h>
#include <parlay/internal/get_time.h>

#include "lasso_regression.h"

// **************************************************************
// Driver
// **************************************************************

// Read matlab data file.
// Returns A^T (i.e. the transpose of A, organizized as columns), and y.
auto read_file(const std::string& filename) {
  auto str = parlay::chars_from_file(filename);
  auto tokens = parlay::tokens(str, [] (char c) {return c == '\n' || c == ',';});
  long ny = parlay::chars_to_long(tokens[1]);
  long nx = parlay::chars_to_long(tokens[ny+4]);
  long n = parlay::chars_to_long(tokens[ny+3]);
  if (2*n + ny + 6 != tokens.size()) {
    std::cout << "bad file format" << std::endl;
    abort();
  }

  auto y = parlay::tabulate(ny, [&] (long i) {
    return parlay::chars_to_double(tokens[i+2]);});

  auto entries = parlay::tabulate(n, [&] (long i) {
    long a = parlay::chars_to_long(tokens[ny+6 + 2*i])-1;
    double v = parlay::chars_to_double(tokens[ny+6 + 2*i + 1]);
    if (a/ny >= nx) {std::cout << a/ny << ", " << i << std::endl; abort();}
    return std::pair{a/ny, non_zero{a%ny, v}};});

  return std::pair(parlay::group_by_index(entries, nx), y);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: lasso_regression <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    auto [AT, y] = read_file(argv[1]);
    parlay::internal::timer t("Time");
    solve_lasso(AT, y, 0.5, 0.0);
    t.next("lasso_regression");
  }
}
