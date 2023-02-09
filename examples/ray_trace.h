#include <limits>
#include <algorithm>
#include "parlay/primitives.h"
#include "box_kdtree.h"

// **************************************************************
// Traces a set of rays to the first triangle they intersect (ray casting).
// Triangles must be stored in a kd-tree, where every triangle must appear
// in every leaf of the tree for which it intersects.
// Can use the the surface area heuristic (SAH) to build the tree, but
// any method will work.
// See box_kdtree for format of tree.
// **************************************************************

const double epsilon=0.00000001;

using coord = double;
struct vect3d {
  std::array<coord,3> a;
  vect3d operator-(vect3d b) const {
    return vect3d(a[0]-b.a[0], a[1]-b[1], a[2]-b[2]);}
  vect3d operator+(vect3d b) const {
    return vect3d(a[0]+b[0], a[1]+b[1], a[2]+b[2]);}
  vect3d operator*(coord b) const {
    return vect3d(a[0]*b, a[1]*b, a[2]*b);}
  coord dot(vect3d b) const {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];}
  vect3d cross(vect3d b) const {
    return vect3d(a[1]*b[2] - a[2]*b[1],
		a[2]*b[0] - a[0]*b[2],
		a[0]*b[1] - a[1]*b[0]); }
  coord& operator[](int i) {return a[i];};
  vect3d() : a({0.0,0.0,0.0}) {}
  vect3d(coord x, coord y, coord z) : a({x,y,z}) {}
};
using point3d = vect3d;
using ray = std::pair<point3d,vect3d>;
using Rays = parlay::sequence<std::pair<point3d,vect3d>>;
using triangle = std::array<point3d,3>;
using Triangles = parlay::sequence<std::array<point3d,3>>;
using point2d = std::array<coord,2>;

range get_range(coord c0, coord c1, coord c2) {
  coord minv = std::min(c0, std::min(c1, c2));
  coord maxv = std::max(c0, std::max(c1, c2));
  if (minv == maxv) return range{(float) minv, (float) (minv+epsilon)};
  else return range{(float) minv, (float) maxv};
}

inline bool in_box(point3d p, Bounding_Box B) {
  return (p[0] >= (B[0][0] - epsilon) && p[0] <= (B[0][1] + epsilon) &&
	  p[1] >= (B[1][0] - epsilon) && p[1] <= (B[1][1] + epsilon) &&
	  p[2] >= (B[2][0] - epsilon) && p[2] <= (B[2][1] + epsilon));
}

// Code for ray_triangle_intersect is based on:
// Fast, Minimum Storage Ray/Triangle Intersection
// Tomas Moller and Ben Trumbore
coord ray_triangle_intersect(ray r, triangle tri) {
  auto [o, d] = r;
  vect3d e1 = tri[1] - tri[0];
  vect3d e2 = tri[2] - tri[0];
  vect3d pvec = d.cross(e2);
  coord det = e1.dot(pvec);

  // if determinant is zero then ray is
  // parallel with the triangle plane
  if (det > -epsilon && det < epsilon) return 0;
  coord invDet = 1.0/det;

  // calculate distance from tri[0] to origin
  vect3d tvec = o - tri[0];

  // u and v are the barycentric coordinates
  // in triangle if u >= 0, v >= 0 and u + v <= 1
  coord u = tvec.dot(pvec) * invDet;

  // check against one edge and opposite point
  if (u < 0.0 || u > 1.0) return 0;

  vect3d qvec = tvec.cross(e1);
  coord v = d.dot(qvec) * invDet;

  // check against other edges
  if (v < 0.0 || u + v > 1.0) return 0;

  //distance along the ray, i.e. intersect at o + t * d
  return e2.dot(qvec) * invDet;
}

// Given an a ray, a bounding box, and a sequence of triangles, returns the 
// index of the first triangle the ray intersects inside the box.
// The triangles are given by n indices I into the triangle array Tri.
// -1 is returned if there is no intersection
index_t find_ray(ray r,
		 parlay::sequence<index_t> const &I, 
		 Triangles const &Tri,
		 Bounding_Box B) {
  auto [o, d] = r;
  index_t n = I.size();
  coord tMin = std::numeric_limits<coord>::max();
  index_t k = -1;
  for (size_t i = 0; i < n; i++) {
    index_t j = I[i];
    coord t = ray_triangle_intersect(r, Tri[j]);
    if (t > 0.0 && t < tMin && in_box(o + d*t, B)) {
      tMin = t;
      k = j;
    }
  }
  return k;
}

// Given a ray and a tree node find the index of the first triangle the 
// ray intersects inside the box represented by that node.
// -1 is returned if there is no intersection
index_t find_ray(ray r, tree_node* TN, const Triangles &triangles) {
  if (TN->is_leaf()) 
    return find_ray(r, TN->box_indices, triangles, TN->box);
  auto [o, d] = r;

  // intersect ray with splitting plane
  int k0 = TN->cut_dim;
  int k1 = (k0 == 2) ? 0 : k0+1;
  int k2 = (k0 == 0) ? 2 : k0-1;
  point2d o_p{o[k1], o[k2]};
  point2d d_p{d[k1], d[k2]};
  // does not yet deal with d[k0] == 0
  coord scale = (TN->cut_off - o[k0])/d[k0];
  point2d p_i{o_p[0] + d_p[0] * scale, o_p[1] + d_p[1] * scale};
  range rx = TN->box[k1];
  range ry = TN->box[k2];
  coord d_0 = d[k0];

  // decide which of the two child boxes the ray intersects
  enum {LEFT, RIGHT, BOTH};
  int recurseTo = LEFT;
  if      (p_i[0] < rx[0]) { if (d_p[0]*d_0 > 0) recurseTo = RIGHT;}
  else if (p_i[0] > rx[1]) { if (d_p[0]*d_0 < 0) recurseTo = RIGHT;}
  else if (p_i[1] < ry[0]) { if (d_p[1]*d_0 > 0) recurseTo = RIGHT;}
  else if (p_i[1] > ry[1]) { if (d_p[1]*d_0 < 0) recurseTo = RIGHT;}
  else recurseTo = BOTH;

  if (recurseTo == RIGHT) return find_ray(r, TN->right, triangles);
  else if (recurseTo == LEFT) return find_ray(r, TN->left, triangles);
  else if (d_0 > 0) {
    index_t t = find_ray(r, TN->left, triangles);
    if (t >= 0) return t;
    else return find_ray(r, TN->right, triangles);
  } else {
    index_t t = find_ray(r, TN->right, triangles);
    if (t >= 0) return t;
    else return find_ray(r, TN->left, triangles);
  }
}

parlay::sequence<index_t> ray_cast(const Triangles &triangles, const Rays &rays) {
  index_t num_rays = rays.size();
  
  // Create box around each triangle
  auto boxes = parlay::tabulate(triangles.size(), [&] (size_t i) {
      auto [p0,p1,p2] = triangles[i];
      return Bounding_Box{get_range(p0[0],p1[0],p2[0]),get_range(p0[1],p1[1],p2[1]),
	  get_range(p0[2],p1[2],p2[2])};});
  
  tree_node* R = kdtree_from_boxes(boxes);

  // get the intersections
  auto results = parlay::tabulate(num_rays, [&] (size_t i) -> index_t {
      return find_ray(rays[i], R, triangles);}, 100);

  tree_node::node_allocator.retire(R);
  return results;
}
