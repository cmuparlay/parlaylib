#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>

// **************************************************************
// Fits a set of points to a line minimizing chi-squared.
// Returns the y intercept at x=0 and the slope.
// Parallel version of the "fit" algorithm from:
// "Numerical Recipes: The art of scientific computing"
// by Press, Teukolsky, Vetterling, and Flannery, section 15.2.
// **************************************************************

using point = std::pair<double,double>;
auto add_point = parlay::pair_monoid(parlay::addm<double>(),
                                     parlay::addm<double>());

// The algorithm
template <class Seq>
auto linefit(const Seq& points) {
  long n = points.size();
  auto [xsum, ysum] = parlay::reduce(points, add_point);
  double xa = xsum/n;
  double ya = ysum/n;
  auto tmp = parlay::delayed_map(points,[=] (point p) {
    auto [x, y] = p;
    double v = x - xa;
    return point(v * v, v * y);});
  auto [Stt, bb] = parlay::reduce(tmp, add_point);
  double b = bb / Stt;
  double a = ya - xa * b;
  return point(a, b);
}

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: linefit <num_points>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    double offset = 1.0;
    double slope = 1.0;
    parlay::random_generator gen;
    std::uniform_real_distribution<> dis(0.0,1.0);

    // generate points on a line
    auto pts = parlay::tabulate(n, [&] (long i) {
	auto r = gen[i];
	double x = dis(r);
	return point(x, offset + x * slope);
    });
    auto [offset_, slope_] = linefit(pts);
    std::cout << "offset = " << offset_ << " slope = " << slope_ << std::endl;
  }
}
