#include <parlay/primitives.h>
#include <parlay/random.h>

// **************************************************************
// Kmeans using Lloyd's algorithm
// **************************************************************

// **************************************************************
// Helper definitions and functions
// **************************************************************

using Point = parlay::sequence<double>;
using Points = parlay::sequence<Point>;

// the 100 in the following two is for granularity control
// i.e. if the the number of dimensions is less than 100, run sequentially
Point operator/(Point& a, double b) {
  return parlay::map(a, [=] (double v) {return v/b;}, 100);}

Point operator+(Point& a, Point& b) {
  return parlay::tabulate(a.size(), [&] (long i) {return a[i] + b[i];}, 100);}

double squared_dist(Point& a, Point& b) {
  return parlay::reduce(parlay::delayed_tabulate(a.size(), [&] (long i) {
	auto d = a[i]-b[i];
	return d*d;}));
}

long closest_point(Point& p, Points& kpts) {
  auto a = parlay::delayed_map(kpts, [&] (Point &q) {
      return squared_dist(p, q);});
  return min_element(a) - a.begin();
}

// monoid is an associative "sum" function and an identity
auto addpair = [] (std::pair<Point,long> a, std::pair<Point,long> b) {
  return std::pair(a.first + b.first, a.second + b.second);};
auto addm = parlay::make_monoid(addpair, std::pair(Point(),0l));

// **************************************************************
// This is the main algorithm
// It uses k random points from the input as the starting centers
// **************************************************************
Points kmeans(Points& pts, int k, int rounds) {
  long n = pts.size();
  pts = parlay::random_shuffle(pts);
  Points kpts = parlay::to_sequence(pts.head(k));

  for (int i = 0; i < rounds; i++) {
    // for each point find closest among the k centers (kpts)
    auto closest = parlay::map(pts, [&] (Point& p) {
	return std::pair{closest_point(p, kpts), std::pair{p,1l}};});

    // Sum the points that map to each of the k centers
    auto mid_and_counts = parlay::reduce_by_index(closest, k, addm);

    // Calculate new centers (average of the points)
    auto kpts = parlay::map(mid_and_counts, [&] (auto mcnt) {
		  return (mcnt.first / (double) mcnt.second);});
  }
  return kpts;
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
    int rounds = 10;
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    parlay::random rand;
    Points pts = parlay::tabulate(n, [&] (long i) {
	return parlay::tabulate(dims, [&] (long j) {
	    return double(rand[i*dims+j] % 1000000);});});
    auto result = kmeans(pts, k, rounds);
    std::cout << "number of rounds: " << rounds << std::endl;
  }
}
