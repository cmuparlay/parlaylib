#include <utility>

#include <parlay/primitives.h>
#include <parlay/delayed.h>

// **************************************************************
// Fits a set of points to a line minimizing chi-squared.
// Returns the y intercept at x=0 and the slope.
// Parallel version of the "fit" algorithm from:
// "Numerical Recipes: The art of scientific computing"
// by Press, Teukolsky, Vetterling, and Flannery, section 15.2.
// **************************************************************

using point = std::pair<double,double>;

// a binary associative operator that elementwise adds two points
auto f = [] (point a, point b) {
  return point{a.first + b.first, a.second + b.second};};
auto add_points = parlay::binary_op(f, point(0,0));

// The algorithm
template <class Seq>
auto linefit(const Seq& points) {
  long n = points.size();
  auto [xsum, ysum] = parlay::reduce(points, add_points);
  double xa = xsum/n;
  double ya = ysum/n;
  auto tmp = parlay::delayed::map(points,[=] (point p) {
    auto [x, y] = p;
    double v = x - xa;
    return point(v * v, v * y);});
  auto [Stt, bb] = parlay::reduce(tmp, add_points);
  double b = bb / Stt;
  double a = ya - xa * b;
  return point(a, b);
}
