#include <iterator>
#include <utility>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// Linear programming in 2d
// Maximize c^T x
// Constrained by Ax <= b
// Each constraint (h_i) is represented as a triple (A_{i,0}, A_{i,1}, b_i).
// 
// Uses the linear work and O(log^2) span with randomized parallel algorithm from:
//   Parallelism in Randomized Incremental Algorithms.
//   Blelloch, Gu, Shun, Sun.
//   JACM, 2020.
// which in turn is based on the sequential algorithm in:
//   Small-dimensional linear programming and convex hulls made easy.
//   Seidel.
//   Discrete & Computational Geometry, 1991.
// **************************************************************

using coord = double;
coord infty = std::numeric_limits<coord>::max();
using constraint = std::array<coord, 3>;
using constraints = parlay::sequence<std::array<coord, 3>>;
using point = std::array<coord, 2>;

// **************************************************************
// Some helper functions
// **************************************************************
template <typename c>
coord cross(c a, constraint b) { return a[0]*b[1] - a[1]*b[0];}
template <typename c>
coord dot(c a, constraint b) { return a[0]*b[0] + a[1]*b[1];}

// check if point p violates the constraint h
bool violate(point p, constraint h) { return dot(p,h) > h[2];}

// intersection point of two (non-parallel) constraints
point intersect(constraint a, constraint b) {
  coord d = 1.0/cross(a,b);
  return point{(b[1]*a[2] - a[1]*b[2])*d,
               (a[0]*b[2] - b[0]*a[2])*d};}

/// projects inequality constraint b onto equality constraint a
coord project(constraint a, constraint b) {
  return cross(intersect(a,b), a);}

// **************************************************************
// The main algorithm
// **************************************************************
auto linear_program_2d(const constraints& H_in, constraint c) {
  // remove constraints that face away from c, and random shuffle
  constraints Hx = parlay::filter(H_in, [&] (constraint h) {
      return dot(h,c) > 0;});
  constraints H = parlay::random_shuffle(Hx);
  long n = H.size();

  // find two bounding constraints and swap to front of H
  auto left = parlay::find_if(H, [&] (constraint h) {return cross(h,c) > 0;});
  if (left - H.begin() == n) return point{infty,infty}; // unbounded
  swap(*left, H[0]);  
  auto right = parlay::find_if(H, [&] (constraint h) {return cross(h,c) < 0;});
  if (right - H.begin() == n) return point{infty,infty}; // unbounded
  swap(*right, H[1]);

  // p keeps current best point, i is the index of constraints considered so far
  point p = intersect(H[0], H[1]);
  long i = 2;

  // a "doubling" search, takes O(log n) rounds whp
  while (i < n) {
    // double size
    long top = std::min(2*i, n);
    // check for a violating constraint from i to top
    long loc = parlay::reduce(parlay::delayed_tabulate(top-i, [&] (long j) {
	  return violate(p, H[i+j]) ? i+j : n;}), parlay::minimum<long>());

    if (loc == n) i = top; // no violing constraint found, repeat and double again
    else { // found a violating constraint at location loc
      // select constraints h up to loc that jointly with H[loc] bound the solution
      // i.e. H[loc] x c and h x c have opposite signs
      coord cr = cross(H[loc],c);
      auto Hf = parlay::filter(H.cut(0,loc), [&] (constraint h) {
	  return cr * cross(h,c) < 0;});

      // find the tightest such constraint
      auto min = [&] (constraint a, constraint b) {
	return cr * (project(H[loc], a) - project(H[loc], b)) > 0 ? a : b;};
      constraint cx = parlay::reduce(Hf, parlay::binary_op(min,constraint{0.,0.,0.}));

      // update the optimal point and the index i
      p = intersect(H[loc], cx);
      i = loc + 1;
    }
  }
  return p;
}
