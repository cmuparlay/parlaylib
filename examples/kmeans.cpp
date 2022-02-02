#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/io.h>

// **************************************************************
// Kmeans using Lloyd's algorithm
// **************************************************************

// **************************************************************
// Helper definitions and functions on points
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

double distance_squared(const Point& a, const Point& b) {
  return parlay::reduce(parlay::delayed_tabulate(a.size(), [&] (long i) {
	auto d = a[i]-b[i];
	return d*d;}));
}

long closest_point(const Point& p, const Points& kpts) {
  auto a = parlay::delayed_map(kpts, [&] (const Point &q) {
      return distance_squared(p, q);});
  return min_element(a) - a.begin();
}

// monoid is an associative "sum" function and an identity
auto addpair = [] (const std::pair<Point,long> &a,
		   const std::pair<Point,long> &b) {
  return std::pair(a.first + b.first, a.second + b.second);};
auto addm = parlay::make_monoid(addpair, std::pair(Point(), 0l));

// **************************************************************
// This is the main algorithm
// It uses k random points from the input as the starting centers
// **************************************************************
auto kmeans(Points& pts, int k, double epsilon) {
  long n = pts.size();
  pts = parlay::random_shuffle(pts);
  // initial k points as k centers
  Points kpts = parlay::to_sequence(pts.head(k));
  long rounds = 0;
  
  while (true) {
    // for each point find closest among the k centers (kpts)
    auto closest = parlay::map(pts, [&] (const Point& p) {
	return std::pair{closest_point(p, kpts), std::pair{p,1l}};});

    // Sum the points that map to each of the k centers
    auto mid_and_counts = parlay::reduce_by_index(closest, k, addm);

    // Calculate new centers (average of the points)
    auto new_kpts = parlay::map(mid_and_counts, [&] (auto mcnt) {
		  return (mcnt.first / (double) mcnt.second);});

    // compare to previous and quit if close enough
    auto ds = parlay::tabulate(k, [&] (long i) {
	        return distance_squared(kpts[i], new_kpts[i]);});
    if (rounds++ < 1000 && sqrt(parlay::reduce(ds)) < epsilon)
      return std::make_pair(new_kpts, rounds);

    kpts = new_kpts;
  }
}

// **************************************************************
// Driver
// **************************************************************
int main(int argc, char* argv[]) {
  auto usage = "Usage: kmeans <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    int dims = 10;
    int k = 10;
    double epsilon = .005;
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random rand;
    Points pts = parlay::tabulate(n, [&] (long i) {
	return parlay::tabulate(dims, [&] (long j) {
	    return double(rand[i*dims+j] % 1000000)/1000000.;});});
    auto [result, rounds] = kmeans(pts, k, epsilon);
    std::cout << rounds << " rounds until diff < " << epsilon << std::endl;
  }
}
