#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <random>

#include <parlay/monoid.h>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// Kmeans using kmeans++ algorithm
// The kmeans++ algorithm is Lloyd's iterative algorithm
// but seeded carefully with points that are spread out.
// **************************************************************

// **************************************************************
// Functions on points
// **************************************************************

using Point = parlay::sequence<double>;
using Points = parlay::sequence<Point>;

// the 100 in the following two is for granularity control
// i.e. if the the number of dimensions is less than 100, run sequentially
Point operator/(const Point& a, const double b) {
  return parlay::map(a, [=] (double v) {return v/b;}, 100);}

Point operator+(const Point& a, const Point& b) {
  if (a.size() == 0) return b;
  return parlay::tabulate(a.size(), [&] (long i) {return a[i] + b[i];}, 100);}

template <typename D>
long closest_point(const Point& p, const Points& kpts, D& distance) {
  auto a = parlay::delayed::map(kpts, [&] (const Point &q) {
    return distance(p, q);});
  return min_element(a) - a.begin();
}

// a monoid is an associative "sum" function along with an identity
auto addpair = [] (const std::pair<Point,long> &a,
                   const std::pair<Point,long> &b) {
  return std::pair(a.first + b.first, a.second + b.second);};
auto addm = parlay::binary_op(addpair, std::pair(Point(), 0l));

// **************************************************************
// This is the main algorithm
// It uses k random points from the input as the starting centers
// **************************************************************
template <typename D>
auto kmeans(Points& pts, int k, D& distance, double epsilon) {
  long n = pts.size();
  long max_round = 1000;
  parlay::random_generator rand;
  std::uniform_real_distribution<> dis(0.0,1.0);

  // add k initial points by the kmeans++ rule
  // random initial center
  Points kpts({pts[rand()%k]});
  for (int i=1; i < k; i++) {
    // find the closest center for all points
    auto dist = parlay::map(pts, [&] (const Point& p) {
      return distance(p, kpts[closest_point(p, kpts, distance)]);});

    // pick with probability proportional do distance (squared)
    auto [sums, total] = scan(dist);
    auto pos = dis(rand) * total;
    auto j = std::upper_bound(sums.begin(), sums.end(), pos) - sums.begin()-1;

    // add to the k points
    kpts.push_back(pts[j]);
  }

  long round = 0;
  while (true) {
    // for each point find closest among the k centers (kpts)
    auto closest = parlay::map(pts, [&] (const Point& p) {
      return std::pair{closest_point(p, kpts, distance),
                       std::pair{p,1l}};});

    // Sum the points that map to each of the k centers
    auto mid_and_counts = parlay::reduce_by_index(closest, k, addm);

    // Calculate new centers (average of the points)
    auto new_kpts = parlay::map(mid_and_counts, [&] (auto mcnt) {
      return (mcnt.first / (double) mcnt.second);});

    // compare to previous and quit if close enough
    auto ds = parlay::tabulate(k, [&] (long i) {
      return distance(kpts[i], new_kpts[i]);});
    if (round++ < max_round && sqrt(parlay::reduce(ds)) < epsilon)
      return std::make_pair(new_kpts, round);

    kpts = new_kpts;
  }
}

// **************************************************************
// Driver
// **************************************************************
double euclidean_squared(const Point& a, const Point& b) {
  return parlay::reduce(parlay::delayed_tabulate(a.size(), [&] (long i) {
    auto d = a[i]-b[i];
    return d*d;}));
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: kmeans <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    int dims = 10;
    int k = 10;
    double epsilon = .005;
    long n;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen;
    std::uniform_real_distribution<> dis(0.0,1.0);

    Points pts = parlay::tabulate(n, [&] (long i) {
      return parlay::tabulate(dims, [&] (long j) {
        auto r = gen[i*dims + j];
        return dis(r);});});
    auto [result, rounds] = kmeans(pts, k, euclidean_squared, epsilon);
    std::cout << rounds << " rounds until diff < " << epsilon << std::endl;
  }
}
