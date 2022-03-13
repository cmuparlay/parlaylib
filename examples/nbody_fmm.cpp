#include <iostream>
#include <string>
#include <random>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>

#include "nbody_fmm.h"

// **************************************************************
// Driver code
// **************************************************************

// checks 500 randomly selected points for accuracy
void check_accuracy(sequence<particle>& p) {
  long n = p.size();
  long nCheck = std::min<long>(n, 500);
  parlay::random_generator gen(123);
  std::uniform_int_distribution<long> dis(0, n-1);

  auto errors = parlay::tabulate(nCheck, [&] (long i) {
    auto r = gen[i];
    long idx = dis(r);
    vect3d force;
    for (long j=0; j < n; j++)
      if (idx != j) {
        vect3d v = (p[j].pt) - (p[idx].pt);
        double r = v.length();
        force = force + (v * (p[j].mass * p[idx].mass / (r*r*r)));
      }
    return (force - p[idx].force).length()/force.length();});

  double rms_error = std::sqrt(parlay::reduce(parlay::map(errors, [] (auto x) {return x*x;}))/nCheck);
  double max_error = parlay::reduce(errors, parlay::maximum<double>());
  std::cout << "  Sampled RMS Error: " << rms_error << std::endl;
  std::cout << "  Sampled Max Error: " << max_error << std::endl;
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: nbody_fmm <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }

    // generate n random particles in a square around the origin with
    // random masses from 0 to 1.
    parlay::random_generator gen(0);
    std::uniform_real_distribution<> box_dis(-1.0,1.0);
    std::uniform_real_distribution<> mass_dis(0.0,1.0);
    auto particles = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      point3d pt{box_dis(r), box_dis(r), box_dis(r)};
      return particle{pt, mass_dis(r), vect3d()};});

    parlay::internal::timer t("Time");
    for (int i=0; i < 1; i++) {
      forces(particles, ALPHA);
      t.next("TOTAL");
    }
    check_accuracy(particles);
  }
}

