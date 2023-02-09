#include <utility>
#include <memory>
#include <optional>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>

#include "hash_map.h"

// **************************************************************
// Parallel Delaunay Triangulation
// From the paper:
// Randomized Incremental Convex Hull is Highly Parallel
// Blelloch, Gu, Shun and Sun
// **************************************************************

using real = float;
using point_id = int;

struct point {
  point_id id; real x; real y;
  const bool operator<(const point p) const {return id < p.id;}
};

using tri = std::array<point_id,3>;
using edge = std::array<point_id,2>;

struct triangle {
  tri t;
  parlay::sequence<point> conflicts;
  triangle();
  triangle(tri t, parlay::sequence<point> c) : t(t), conflicts(c) {}
};

// A "staged" version of in_cicle.
// It takes 3 points, returning a function on one more point that
// reports whether that point is in the circumcircle of the points.
// Since we often check multiple points against a single circle
// this reduces the work.
// Projects onto a parobola and does a planeside test.
auto in_circle (point a, point b, point d) {
  struct vect { double x,y,z;};
  auto project = [=] (point p) {
    double px = p.x-d.x; double py = p.y-d.y;
    return vect{px, py, px*px + py*py};};
  auto cross = [] (vect v1, vect v2) {
    return vect{v1.y*v2.z - v1.z*v2.y,
                v1.z*v2.x - v1.x*v2.z,
                v1.x*v2.y - v1.y*v2.x};};
  vect cp = cross(project(a), project(b));
  return [=] (point c) -> bool{
    auto dot = [] (vect v1, vect v2) {
      return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;};
    return dot(cp, project(c)) > 0.0;};
}

// **************************************************************
// The main body
// **************************************************************

using Points = parlay::sequence<point>;

struct Delaunay {
  using triangle_ptr = std::shared_ptr<triangle>;
  hash_map<tri,bool> mesh;
  hash_map<edge,triangle_ptr> edges;
  Points points;
  point_id n;

  point_id earliest(triangle_ptr& t) {
    return (t->conflicts.size() == 0) ? n : t->conflicts[0].id;}

  // filter points in the circumcicle of t1 and t2 which are in the
  // circumcicle of t
  auto filter_points(triangle_ptr& t1, triangle_ptr& t2, tri t) {
    auto a = parlay::merge(t1->conflicts, t2->conflicts);
    auto is_in_circle = in_circle(points[t[0]],points[t[1]],points[t[2]]);
    auto keep = parlay::tabulate(a.size(), [&] (long i) {
      return ((i != 0) && (a[i].id != a[i-1].id) &&
              ((i+1 < a.size() && a[i].id == a[i+1].id) ||
               is_in_circle(a[i])));},500);
    return parlay::pack(a, keep);
  }

  // recursive routine
  // almost direct transcription from paper but limited to
  // 2d Delaunay instead of arbitrary convex hull
  void process_edge(triangle_ptr& t1, edge e, triangle_ptr& t2) {
    if (t1->conflicts.size() == 0 && t2->conflicts.size() == 0) {
      mesh.insert(t1->t,true); mesh.insert(t2->t,true);
      t1 = t2 = nullptr;
    } else if (earliest(t2) == earliest(t1)) {
      t1 = t2 = nullptr;
    } else {
      if (earliest(t2) < earliest(t1)) {
        std::swap(t2, t1); std::swap(e[0], e[1]);}
      point_id p = earliest(t1);
      tri t{e[0], e[1], p};
      t1 = std::make_shared<triangle>(t, filter_points(t1, t2, t));
      auto check_edge = [&] (edge e, triangle_ptr& tp) {
        auto key = (e[0] < e[1]) ? e : edge{e[1], e[0]};
        if (edges.insert(key, tp)) return;
        auto tt = *edges.remove(key);
        process_edge(tp, e, tt);};
      auto ta1 = t1; auto tb1 = t1;
      parlay::par_do3([&] {check_edge(edge{p, e[0]}, ta1);},
                      [&] {check_edge(edge{e[1], p}, tb1);},
                      [&] {process_edge(t1, e, t2);});
    }
  }

  // toplevel routine
  // The result is left in the hashmap mesh.  It includes all
  // triangles in the final mesh.
  // Assumes points are inside unit square to fit enclosing triangle.
  // And need to be in randomized order for good efficiency.
  Delaunay(const Points& P) :
      mesh(hash_map<tri,bool>(2*P.size())),
      edges(hash_map<edge,triangle_ptr>(6*P.size())),
      n(P.size()) {
    points = P;
    // enclosing triangle
    point p0{n,0.0,100.0};
    point p1{n+1,100.0,-100.0};
    point p2{n+2,-100.0,-100.0};
    points = parlay::append(P, Points({p0, p1, p2}));
    auto t = std::make_shared<triangle>(tri{n, n+1, n+2}, P);
    n += 3;
    auto te = std::make_shared<triangle>(tri{-1, -1, -1}, Points());
    std::shared_ptr<triangle> te2, te3, t2, t3;
    t2 = t3 = t;
    te2 = te3 = te;
    parlay::par_do3([&] {process_edge(t2, edge{p0.id,p1.id}, te2);},
                    [&] {process_edge(t3, edge{p1.id,p2.id}, te3);},
                    [&] {process_edge(t, edge{p2.id,p0.id}, te);});
  }
};

parlay::sequence<tri> delaunay(const Points& P) {
  Delaunay dt(P);
  return dt.mesh.keys();
}
