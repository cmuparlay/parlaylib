#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "2d_linear_program.h"

// **************************************************************
// Driver code
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: 2d_linear_program <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_real_distribution<coord> dis(-1.0,1.0);
    std::uniform_real_distribution<coord> pos_dis(0.0,1.0);

    // generate n "random" constraints a x + b y <= c
    // Each c is non-negative so the origin always satisfies
    // the constraints (i.e. always feasible).
    constraints H = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      return constraint{dis(r), dis(r), pos_dis(r)};});

    // the objective, in a ranom direction
    //auto c = constraint{dis(gen), dis(gen), 0.0};
    auto c = constraint{0.0, 1.0, 0.0};
    point result;

    parlay::internal::timer t("Time");
    for (int i=0; i < 3; i++) {
      result = linear_program_2d(H, c);
      t.next("2d_linear_program");
    }

    std::cout << "optimal point = " << result[0] << ", " << result[1] << std::endl;
  }
}
