#include <array>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "hash_map.h"

// **************************************************************
// Parallel Convel Hull in 3D
// From the paper:
// Randomized Incremental Convex Hull is Highly Parallel
// Blelloch, Gu, Shun and Sun
//
// Author: Jekyeom Jeon
// **************************************************************

using real = float;
using point_id = int;

using tri = std::array<point_id,3>;
using edge = std::array<point_id,2>;

struct point {
  point_id id; real x; real y; real z;
  const bool operator<(const point p) const {return id < p.id;}
};

struct vect {
  double x; double y; double z;
  vect(double x, double y, double z) : x(x), y(y), z(z) {}
};

struct triangle {
  tri t;
  point_id pid; // point in convex hull not in triangle
  parlay::sequence<point> conflicts;
  triangle();
  triangle(tri t, point_id pid, parlay::sequence<point> c) : t(t), pid(pid), conflicts(c) {}
};

// A "staged" version of a test if two points (p and q) are on the
// opposite side of a plane defined by three other points (a, b, c).
// It first takes the 4 points (a, b, c, p) and returns a function
// which takes the point q, answering the query.  Since we often check
// multiple points q for being opposite to p, this reduces the work.
// Works by taking the cross product of c->a and c->b and checking that
// the dot of this with c->p and c->q have opposite signs.
inline auto is_opposite (point a, point b, point c, point p) {
  auto minus = [] (point p1, point p2) { return vect(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);};
  auto dot = [] (vect v1, vect v2) { return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;};
  auto cross = [] (vect v1, vect v2) {
    return vect{v1.y*v2.z - v1.z*v2.y,
                v1.z*v2.x - v1.x*v2.z,
                v1.x*v2.y - v1.y*v2.x};};
  vect cp = cross(minus(a, c), minus(b, c));
  bool p_side = dot(cp, minus(p, c)) > 0.0;
  return [=] (point q) -> bool {
    bool q_side = dot(cp, minus(q, c)) > 0.0;
    return (q_side != p_side);};
}

// **************************************************************
// The main body
// **************************************************************

using Points = parlay::sequence<point>;

struct Convex_Hull_3d {
  using triangle_ptr = std::shared_ptr<triangle>;
  hash_map<edge, triangle_ptr> map_facets;
  hash_map<tri, bool> convex_hull;
  Points points;
  point_id n;

  point_id min_conflicts(triangle_ptr& t) {
    return (t->conflicts.size() == 0) ? n : t->conflicts[0].id;}

  // recursive routine
  // convex hull in 3d space
  void process_ridge(triangle_ptr& t1, edge r, triangle_ptr& t2) {
    if (t1->conflicts.size() == 0 && t2->conflicts.size() == 0) {
      return;
    } else if (min_conflicts(t2) == min_conflicts(t1)) {
      // H \ {t1, t2}
      convex_hull.remove(t1->t);
      convex_hull.remove(t2->t);
    } else if (min_conflicts(t2) < min_conflicts(t1)) {
      process_ridge(t2, r, t1);
    } else {
      point_id pid = min_conflicts(t1);
      tri t{r[0], r[1], pid};

      // C(t) <- v' in C(t1) U C(t2) | visible(v', t)
      auto t1_U_t2 = parlay::merge(t1->conflicts, t2->conflicts);

      // removes first point, removes duplicates, and checks for visibility
      auto test = is_opposite(points[t[0]], points[t[1]], points[t[2]], points[t1->pid]);
      auto keep = parlay::tabulate(t1_U_t2.size(), [&] (long i) {
          return (((i != 0) && (t1_U_t2[i].id != t1_U_t2[i-1].id)) &&
                  test(t1_U_t2[i]));});
      auto p = parlay::pack(t1_U_t2, keep);

      auto t_new = std::make_shared<triangle>(t, t1->pid, p);
      
      // H <- (H \ {t1}) U {t}
      convex_hull.remove(t1->t);
      convex_hull.insert(t, true);
      
      auto check_edge = [&] (edge e, triangle_ptr& tp) {
        auto key = (e[0] < e[1]) ? e : edge{e[1], e[0]};
        if (map_facets.insert(key, tp)) return;
        auto tt = map_facets.remove(key).value();
        process_ridge(tp, e, tt);};

      auto t_new_1 = t_new; auto t_new_2 = t_new;
      parlay::par_do3([&] {process_ridge(t_new, r, t2);},
                      [&] {check_edge(edge{r[0], pid}, t_new_1);},
                      [&] {check_edge(edge{r[1], pid}, t_new_2);});
    }
  }

  // toplevel routine
  // The result is set of facets: result_facets
  // assumes that P has at least 4 points, and all points are in general position
  Convex_Hull_3d(const Points& P) :
      map_facets(hash_map<edge, triangle_ptr>(6*P.size())),
      convex_hull(hash_map<tri, bool>(6*P.size())),
      n(P.size()) {
             
    points = P;
    // first 4 points are used to define an initial tetrahedron with 4 faces
    tri init_tri[4] = {{0, 1, 2},
                       {1, 2, 3},
                       {0, 2, 3},
                       {0, 1, 3}};
    // points on the inside of each face (i.e. the missing point)
    point_id remain[4] = {3, 0, 1, 2}; 

    // insert the initial convex hull
    for (auto t : init_tri)
      convex_hull.insert(t, true);

    // remove first 4 points
    Points target_points = P.subseq(4, P.size());

    // initialize each of the 4 faces of the tetrahedron
    triangle_ptr t[4];
    parlay::parallel_for(0, 4, [&] (int i) {
        point p0 = points[init_tri[i][0]];
        point p1 = points[init_tri[i][1]];
        point p2 = points[init_tri[i][2]];
        point pc = points[remain[i]];
        auto test = is_opposite(p0, p1, p2, pc);
        t[i] = std::make_shared<triangle>(init_tri[i], remain[i],
                                          parlay::filter(target_points, test));
      });

    // The 6 ridges of the initial tetrahedron along with their neighboring triangles
    std::tuple<triangle_ptr, triangle_ptr, edge> ridges[6] =
      {{t[0], t[1], edge{1, 2}},
       {t[0], t[2], edge{0, 2}},
       {t[0], t[3], edge{0, 1}},
       {t[1], t[2], edge{2, 3}},
       {t[1], t[3], edge{1, 3}},
       {t[2], t[3], edge{0, 3}}};

    parlay::parallel_for(0, 6, [&](auto i) {
        auto [t1, t2, e] = ridges[i];
        process_ridge(t1, e, t2); }, 1);
  }
};

parlay::sequence<tri> convex_hull_3d(const Points& P) {
  Convex_Hull_3d ch3d(P);
  return ch3d.convex_hull.keys();
}
