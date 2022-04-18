#include <iterator>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>
#include <parlay/delayed.h>

// **************************************************************
// The quickhull algorithm for 2d convex hull.
// For a sequence of 2d points, returns the indices of the points on
// the upper hull in left to right order.
// Uses the divide-and-conquer quickhull algorithm.
// **************************************************************

// **************************************************************
// A couple operations on points
// **************************************************************
struct point { double x; double y;};

// twice the area in the triangle defined by three points
// negative if counter clockwise
inline double area(point a, point b, point c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

using intseq = parlay::sequence<int>;
using pointseq = parlay::sequence<point>;

// **************************************************************
// The main recursive function
// Points are all the original points,
// l and r are the start and endpoints to find the hull between, and
// Idxs are the indices of the points (in Points) above the line defined by l--r.
// **************************************************************
intseq quickhull(pointseq const &Points, intseq Idxs, point l, point r) {
  long n = Idxs.size();
  if (n <= 1) return Idxs;

  // find relative distances from the line l--r, tag each with its index
  auto Pairs = parlay::delayed::map(Idxs, [&] (int idx) {
    return std::make_pair(area(l, r, Points[idx]), idx);});

  // get the index of the point with maximum distance from l--r
  auto mid_idx = parlay::reduce(Pairs, parlay::maximum<std::pair<double,int>>()).second;
  point mid = Points[mid_idx];

  // get points above the line l--P[mid_idx]
  auto left = parlay::filter(Idxs, [&] (int id) {
    return area(l, mid, Points[id]) > 0;});

  // get points above the line P[mid_idx]--r
  auto right = parlay::filter(Idxs, [&] (int id) {
    return area(mid, r, Points[id]) > 0;});

  Idxs.clear(); // clear and use std::move to avoid O(n log n) memory usage

  // recurse in parallel
  intseq leftR, rightR;
  parlay::par_do_if(n > 100,
                    [&] () {leftR = quickhull(Points, std::move(left), l, mid);},
                    [&] () {rightR = quickhull(Points, std::move(right), mid, r);});

  parlay::sequence<intseq> nested = {leftR, intseq(1,mid_idx), rightR};
  return parlay::flatten(nested);
}

// **************************************************************
// The top-level call has to find the maximum and minimum x coordinates
//   and use them for the initial recursive call
// **************************************************************
intseq upper_hull(pointseq const &Points) {
  int n = Points.size();
  auto pntless = [&] (point a, point b) {
    return (a.x < b.x) || ((a.x == b.x) && (a.y < b.y));};

  // min and max points by x coordinate
  auto minmax = parlay::minmax_element(Points, pntless);
  auto min_idx = minmax.first - std::begin(Points);
  auto max_idx = minmax.second - std::begin(Points);
  point maxp = Points[max_idx];
  point minp = Points[min_idx];

  // get indices of points above the line P[mid_idx]--P[max_idx]
  auto above = parlay::filter(parlay::iota(n), [&] (int id) {
    return area(minp, maxp, Points[id]) > 0;});

  intseq res = quickhull(Points, std::move(above), minp, maxp);
  parlay::sequence<intseq> nested = {intseq(1, min_idx), res, intseq(1, max_idx)};
  return parlay::flatten(nested);
}
