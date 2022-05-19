#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/io.h"

#include "ray_trace.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: ray_trace <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n = 0;
    int k = 10;
    try { n = std::stol(argv[1]); }
    catch (...) {}
    parlay::random_generator gen(0);
    std::uniform_real_distribution<float> dis(0.0,1.0);
    Triangles triangles;
    //Rays rays;
    point3d minp;
    point3d maxp;
    if (n == 0) { // read triangles from a file
      auto str = parlay::chars_from_file(argv[1]);
      auto words = parlay::tokens(str, [] (char c) {return std::isspace(c);});
      long num_points = parlay::chars_to_long(words[0]);
      long num_triangles = parlay::chars_to_long(words[1]);
      auto coords = parlay::map(words.cut(2,2+3*num_points), [&] (auto s) {
	  return parlay::chars_to_float(s);});
      auto points = parlay::tabulate(num_points, [&] (long i) {
	  return point3d{coords[3*i], coords[3*i+1], coords[3*i+2]};});
      auto minf = [] (point3d a, point3d b) {
	return point3d{std::min(a[0],b[0]),std::min(a[1],b[1]),std::min(a[2],b[2])};};
      auto maxf = [] (point3d a, point3d b) {
	return point3d{std::max(a[0],b[0]),std::max(a[1],b[1]),std::max(a[2],b[2])};};
      minp = parlay::reduce(points, parlay::binary_op(minf,points[0]));
      maxp = parlay::reduce(points, parlay::binary_op(maxf,points[0]));
      auto corners = parlay::map(words.cut(2+3*num_points,words.size()), [&] (auto s) {
	  return parlay::chars_to_long(s);});
      triangles = parlay::tabulate(num_triangles, [&] (long i) {
	  return triangle{points[corners[3*i]-1], points[corners[3*i+1]-1],
			  points[corners[3*i+2]-1]};});
      n = num_triangles;
    } else {  // make up triangles
      minp = point3d{0.0, 0.0, 0.0};
      maxp = point3d{100.0, 100.0, 100.0};
      triangles = parlay::tabulate(n, [&] (long i) {
	  auto r = gen[i];
	  point3d o = {100*dis(r), 100*dis(r), 100*dis(r)};
	  vect3d v1 = {dis(r), dis(r), dis(r)};
	  vect3d v2 = {dis(r), dis(r), dis(r)};
	  return triangle{o, o+v1, o+v2};
	});
    }
    // start random rays from z = zmin plane looking toward z = zmax plane.
    auto rays = parlay::tabulate(n, [&] (long i) {
	auto r = gen[i+n];
	auto d = maxp - minp;
	return ray{point3d{minp[0] + d[0] * dis(r), minp[1] + d[1] * dis(r), minp[2]},
	    point3d{minp[0] + d[0] * dis(r), minp[1] + d[1] * dis(r), maxp[2]}};
      });
    
    
    parlay::sequence<index_t> r;
    parlay::internal::timer t("Time");
    for (int i=0; i < 10; i++) {
      r = ray_cast(triangles, rays);
      t.next("ray_trace");
    }

    long cnt = parlay::reduce(parlay::map(r, [] (index_t i) {return (i < 0) ? 0 : 1;}));
    std::cout << cnt << " rays intersect a triangle out of " << rays.size() << std::endl;
  }
}

