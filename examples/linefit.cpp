#include <parlay/primitives.h>
#include <parlay/random.h>

using point = std::pair<double,double>;
auto add_point = parlay::pair_monoid(parlay::addm<double>(),
				    parlay::addm<double>());

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

int main(int argc, char* argv[]) {
  auto usage = "Usage: linefit <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    try {n = std::stol(argv[1]);}
    catch (...) {std::cout << usage << std::endl; return 1;}
    double offset = 1.0;
    double slope = 1.0;
    parlay::random r;

    // generate points on a line
    auto pts = parlay::tabulate(n, [&] (long i) {
				double x = r[i] % 1000000000000l;
				return point(x, offset + x * slope);
			      });
    auto [offset_, slope_] = linefit(pts);
    std::cout << "offset = " << offset_ << " slope = " << slope_ << std::endl;
  }
}
