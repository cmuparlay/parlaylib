#include <cmath>
#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>
#include <complex>

#include <parlay/alloc.h>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "helper/spherical.h"

using parlay::sequence;

// **************************************************************
// This is an implementation of the Callahan-Kosaraju (CK) algorithm
// for n-body simulation based on multipole expansions.
//
//   Paul Callahan and S. Rao Kosaraju
//   A decomposition of multi-dimensional point-sets with applications 
//   to k-nearest-neighbors and n-body potential fields 
//   ACM Symposium on Theory of Computation, 1992
//
// For bodies experiencing gravitational or electrostatic forces it,
// calculates the forces for each of n-bodies on each other.
// Naively this would take n^2 work, but CK algorithms runs in O(n log n)
// work to build a tree with a small constant, and O(n) work for the
// potential (force) calculations, with a significantly larger constant.
// 
// It uses similar ideas to Greengard and Rothkin's Fast Multipole
// Method (FMM) but is more flexible for unbalanced trees.  As with
// FMM it uses "Multipole" (or "exterior") and "Local" (or "interior")
// expansion on the potential fields.  For the expansions it uses a
// modified version of the multipole translation code from the PETFMM
// library using spherical harmonics.  The translations are
// implemented in spherical.h and can be changed for any other
// representation that supports the public interface of the
// exterior_multipole and interior_mutipole structures.
//
// Similarly to many FMM-based codes it works in the following steps
//   1) build the CK tree recursively (similar to a k-d tree)
//   2) calculate multipole expansions going up the tree
//   3) figure out all far-field interactions using the CK method
//   4) translate all multipole to local expansions along 
//      the far-field interactions calculated in 3).
//   5) propagate local expansions down the tree
//   6) finally add in all direct leaf-leaf interactions
// **************************************************************

// **************************************************************
// The accuracy can be adjusted using the following parameters
//   ALPHA -- Controls distance which is considered far-field.
//            It is the min ratio of the larger of two interacting 
//            boxes to the distance between them
//   terms -- Number of terms in the expansions
// The performance can be adjusted with
//   BOXSIZE -- The max number of particles in each leaf of the tree
//      this also slightly affects accuracy (smaller is better)
// **************************************************************

// Following for 1e-3 rms accuracy
//#define ALPHA 2.2
//#define terms 7
//#define BOXSIZE 125

// Following for 1e-6 rms accuracy (2.5x slower than above)
#define ALPHA 2.6
#define terms 12
#define BOXSIZE 250

// Following for 1e-9 rms accuracy (2.2x slower than above)
// #define ALPHA 3.0
// #define terms 17
// #define BOXSIZE 500

// Following for 1e-12 rms  accuracy (1.8x slower than above)
//#define ALPHA 3.2
//#define terms 22
//#define BOXSIZE 700

// *************************************************************
// Points and vectors in 3d
// *************************************************************
using coord = double;
struct vect3d {
  std::array<coord,3> a;
  vect3d operator-(vect3d b) {
    return vect3d(a[0]-b.a[0], a[1]-b.a[1], a[2]-b.a[2]);}
  vect3d operator+(vect3d b) {
    return vect3d(a[0]+b.a[0], a[1]+b.a[1], a[2]+b.a[2]);}
  vect3d operator*(coord b) {
    return vect3d(a[0]*b, a[1]*b, a[2]*b);}
  coord& operator[](int i) {return a[i];};
  coord length_squared() {return a[0]*a[0]+a[1]*a[1]+a[2]*a[2];}
  coord length() {return sqrt(length_squared());}
  coord max_coord() {return std::max({a[0],a[1],a[2]});}
  vect3d max(vect3d b) {
    return vect3d(std::max(a[0],b.a[0]),std::max(a[1],b.a[1]),std::max(a[2],b.a[2]));}
  vect3d min(vect3d b) {
    return vect3d(std::min(a[0],b.a[0]),std::min(a[1],b.a[1]),std::min(a[2],b.a[2]));}
  vect3d() : a({0.0,0.0,0.0}) {}
  vect3d(coord x, coord y, coord z) : a({x,y,z}) {}
};

using point3d = vect3d;
using box = std::pair<point3d,point3d>;

class particle {
 public:
  point3d pt;
  double mass;
  vect3d force;
};

// Set global constants for spherical harmonics
using transform = Transform<vect3d,terms>;

transform* get_global_transform() {
  static transform TRglobal;
  return &TRglobal;
}

// *************************************************************
//  Exterior multipole expansions
//  To approximate the potential at points far from a center
//  due to points near the center.
// *************************************************************
struct exterior_expansion {
  transform* TR;
  std::complex<double> coefficients[terms*terms];
  point3d center;
  // add contributions from the leaves
  void addTo(point3d pt, double mass) {
    TR->P2Madd(coefficients, mass, center, pt); }

  // Add in another exterior expansion.
  // Used going up the tree to accumuate contributions from children.
  void addTo(exterior_expansion* y) {
    TR->M2Madd(coefficients, center, y->coefficients, y->center);}

  exterior_expansion(transform* TR, point3d center) : TR(TR), center(center) {
    for (long i=0; i < terms*terms; i++) coefficients[i] = 0.0; }
  exterior_expansion() {}
};

parlay::type_allocator<exterior_expansion> exterior_pool;

// *************************************************************
//  Interior multipole expansions (also called local expansions)
//  To approximate the potential at points near a center due to 
//  points far from the center.
// *************************************************************
struct interior_expansion {
  transform* TR;
  std::complex<double> coefficients[terms*terms];
  point3d center;
  // translate from exterior in far box and add to interior here
  void addTo(exterior_expansion* y) {
    TR->M2Ladd(coefficients, center, y->coefficients, y->center);}

  // Add in another interior expansion with a different center.
  // Uses going down the tree to include contributions from parent.
  void addTo(interior_expansion* y) {
    TR->L2Ladd(coefficients, center, y->coefficients, y->center); }

  // calculate the force at a nearby point based on expansion
  vect3d force(point3d y, double mass) {
    vect3d result;
    double potential; // ignored since we just care about force
    TR->L2P(potential, result, y, coefficients, center);
    return result * mass;
  }

  interior_expansion(transform* TR, point3d center) : TR(TR), center(center) {
    for (long i=0; i < terms*terms; i++) coefficients[i] = 0.0; }
  interior_expansion() {}
};

parlay::type_allocator<interior_expansion> interior_pool;

// *************************************************************
//  A node in the CK tree
//  Either a leaf (if children are null) or internal node.
//  If a leaf, will contains a set of particles.
//  If an internal node, contains a left and a right child.
//  All nodes contain exterior and interior expansions.
//  The leftNeighbors and rightNeighbors contain edges in the CK
//  well separated decomposition.
// *************************************************************

struct node {
  using edge = std::pair<node*, long>;
  node* left;
  node* right;
  sequence<particle*> particles;
  sequence<std::pair<vect3d,double>> particles_d;
  long n;
  box b;
  exterior_expansion* ExtExp;
  interior_expansion* IntExp;
  std::vector<node*> indirectNeighbors;
  std::vector<edge> leftNeighbors;
  std::vector<edge> rightNeighbors;
  sequence<sequence<vect3d>> hold;
  bool leaf() {return left == nullptr;}
  node() {}
  point3d center() { return (b.first + b.second) * 0.5;}
  double radius() { return (b.second - b.first).length() * 0.5;}
  double lmax() { return (b.second-b.first).max_coord();}
  void allocateExpansions() {
    ExtExp = exterior_pool.allocate(get_global_transform(), center());
    IntExp = interior_pool.allocate(get_global_transform(), center());
  }
  node(node* L, node* R, long n, box b)
      : left(L), right(R), n(n), b(b) {
    allocateExpansions();
  }
  node(parlay::sequence<particle*> P, box b)
      : left(nullptr), right(nullptr), particles(std::move(P)), b(b) {
    n = particles.size();
    particles_d = parlay::map(particles, [] (auto p) {
      return std::pair{p->pt,p->mass};});
    allocateExpansions();
  }
};

long numLeaves(node* tr) {
  if (tr->leaf()) return 1;
  else return(numLeaves(tr->left)+numLeaves(tr->right));
}

parlay::type_allocator<node> node_pool;

using edge = std::pair<node*, long>;

// *************************************************************
//  Build the CK tree
//  Similar to a kd-tree but always split along widest dimension
//  of the points instead of the next round-robin dimension.
// *************************************************************
template <typename Particles>
node* build_tree(Particles& particles, long effective_size) {
  long n = particles.size();
  long en = std::max(effective_size, n);

  auto minmax = [] (box a, box b) {
    return box((a.first).min(b.first),
               (a.second).max(b.second));};
  auto pairs = parlay::delayed_map(particles, [&] (particle* p) {
    return box(p->pt, p->pt);});
  box b = parlay::reduce(pairs, parlay::binary_op(minmax,pairs[0]));

  if (en < BOXSIZE || n < 10)
    return node_pool.allocate(parlay::to_sequence(particles), b);

  vect3d box_dims = (b.second - b.first);

  int d = 0;
  coord Delta = 0.0;
  for (int i=0; i < 3; i++) {
    if (b.second[i] - b.first[i] > Delta) {
      d = i;
      Delta = b.second[i] - b.first[i];
    }
  }

  double splitpoint = (b.first[d] + b.second[d])/2.0;

  auto isLeft = parlay::delayed_map(particles, [&] (particle* p) {
    return std::pair(p->pt[d] < splitpoint, p);});
  auto foo = parlay::group_by_index(isLeft, 2);
  particles.clear();

  auto r = parlay::map(foo, [&] (auto& x) {
    return build_tree(x, .4 * en);}, 1);
  return node_pool.allocate(r[0], r[1], n, b);
}

// *************************************************************
// Determine if two nodes (boxes) are separated enough to use
// the multipole approximation.
// *************************************************************
bool far_away(node* a, node* b) {
  double rmax = std::max(a->radius(), b->radius());
  double r = (a->center() - b->center()).length();
  return r >= (ALPHA * rmax);
}

// *************************************************************
// Used to count the number of interactions. Just for performance
// statistics not needed for correctness.
// *************************************************************
struct interactions_count {
  long direct;
  long indirect;
  interactions_count() {}
  interactions_count(long a, long b) : direct(a), indirect(b) {}
  interactions_count operator+ (interactions_count b) {
    return interactions_count(direct + b.direct, indirect + b.indirect);}
};

// *************************************************************
// The following two functions are the core of the CK method.
// They calculate the "well separated decomposition" of the points.
// *************************************************************
interactions_count interactions(node* Left, node* Right) {
  if (far_away(Left,Right)) {
    Left->indirectNeighbors.push_back(Right);
    Right->indirectNeighbors.push_back(Left);
    return interactions_count(0,2);
  } else {
    if (!Left->leaf() && (Left->lmax() >= Right->lmax() || Right->leaf())) {
      interactions_count x = interactions(Left->left, Right);
      interactions_count y = interactions(Left->right, Right);
      return x + y;
    } else if (!Right->leaf()) {
      interactions_count x = interactions(Left, Right->left);
      interactions_count y = interactions(Left, Right->right);
      return x + y;
    } else { // both are leaves
      if (Right->n > Left->n) std::swap(Right,Left);
      long rn = Right->leftNeighbors.size();
      long ln = Left->rightNeighbors.size();
      Right->leftNeighbors.push_back(edge(Left,ln));
      Left->rightNeighbors.push_back(edge(Right,rn));
      return interactions_count(Right->n*Left->n,0);
    }
  }
}

// Could be parallelized but would require avoiding push_back.
// Currently not a bottleneck so left serial.
interactions_count interactions(node* tr) {
  if (!tr->leaf()) {
    interactions_count x, y, z;
    x = interactions(tr->left);
    y = interactions(tr->right);
    z = interactions(tr->left,tr->right);
    return x + y + z;
  } else return interactions_count(0,0);
}

// *************************************************************
// Translate from exterior (multipole) expansion to interior (local)
// expansion along all far-field interactions.
// *************************************************************
void do_indirect(node* tr) {
  for (long i = 0; i < tr->indirectNeighbors.size(); i++)
    tr->IntExp->addTo(tr->indirectNeighbors[i]->ExtExp);
  if (!tr->leaf()) {
    parlay::par_do([&] () {do_indirect(tr->left);},
                   [&] () {do_indirect(tr->right);});
  }
}

// *************************************************************
// Translate and accumulate exterior (multipole) expansions up the tree,
// including translating particles to expansions at the leaves.
// *************************************************************
void up_sweep(node* tr) {
  if (tr->leaf()) {
    for (long i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      tr->ExtExp->addTo(P->pt, P->mass);
    }
  } else {
    parlay::par_do([&] () {up_sweep(tr->left);},
                   [&] () {up_sweep(tr->right);});
    tr->ExtExp->addTo(tr->left->ExtExp);
    tr->ExtExp->addTo(tr->right->ExtExp);
  }
}

// *************************************************************
// Translate and accumulate interior (local) expansions down the tree,
// including applying them to all particles at the leaves.
// *************************************************************
void down_sweep(node* tr) {
  if (tr->leaf()) {
    for (long i=0; i < tr->n; i++) {
      particle* P = tr->particles[i];
      P->force = P->force + tr->IntExp->force(P->pt, P->mass);
    }
  } else {
    parlay::par_do([&] () {tr->left->IntExp->addTo(tr->IntExp);
                     down_sweep(tr->left);},
                   [&] () {tr->right->IntExp->addTo(tr->IntExp);
                     down_sweep(tr->right);});
  }
}

// puts the leaves of tree tr into the array Leaves in left to right order
long get_leaves(node* tr, node** Leaves) {
  if (tr->leaf()) {
    Leaves[0] = tr;
    return 1;
  } else {
    long l = get_leaves(tr->left, Leaves);
    long r = get_leaves(tr->right, Leaves + l);
    return l + r;
  }
}

// *************************************************************
// Calculates the direct forces between all pairs of particles in two
// near-field leaf nodes.  Directly updates forces in Left, and places
// forces for ngh in hold.  This avoids a race condition on modifying
// ngh while someone else is updating it.
// *************************************************************
auto direct(node* Left, node* ngh) {
  auto LP = (Left->particles).data();
  auto R = (ngh->particles_d).data();
  long nl = Left->n;
  long nr = ngh->n;
  parlay::sequence<vect3d> hold(nr, vect3d());
  for (long i=0; i < nl; i++) {
    vect3d frc;
    particle pa = *LP[i];
    for (long j=0; j < nr; j++) {
      vect3d v = R[j].first - pa.pt;
      double r2 = v.length_squared();
      vect3d force = (v * (pa.mass * R[j].second / (r2*sqrt(r2))));;
      hold[j] = hold[j] - force;
      frc = frc + force;
    }
    LP[i]->force = LP[i]->force + frc;
  }
  return hold;
}

// *************************************************************
// Calculates local forces within a leaf
// *************************************************************
void self(node* Tr) {
  auto PP = (Tr->particles).data();
  for (long i=0; i < Tr->n; i++) {
    particle* pa = PP[i];
    for (long j=i+1; j < Tr->n; j++) {
      particle* pb = PP[j];
      vect3d v = (pb->pt) - (pa->pt);
      double r2 = v.length_squared();
      vect3d force = (v * (pa->mass * pb->mass / (r2*sqrt(r2))));
      pb->force = pb->force - force;
      pa->force = pa->force + force;
    }
  }
}

// *************************************************************
// Calculates the direct interactions between and within leaves.
// Since the forces are symmetric, this calculates the force on one
// side (rightNeighbors) while storing them away (in hold).
// It then goes over the other side (leftNeighbors) picking up
// the precalculated results (from hold).
// It does not update both sides immediately since that would 
// generate a race condition.
// *************************************************************
void do_direct(node* a) {
  long nleaves = numLeaves(a);
  sequence <node*> Leaves(nleaves);
  get_leaves(a, Leaves.data());

  // calculates interactions and put neighbor's results in hold
  parlay::parallel_for (0, nleaves, [&] (long i) {
    long rn = Leaves[i]->rightNeighbors.size();
    Leaves[i]->hold = parlay::tabulate(rn, [&] (long j) {
      return direct(Leaves[i], Leaves[i]->rightNeighbors[j].first);}, rn);}, 1);

  // picks up results from neighbors that were left in hold
  parlay::parallel_for (0, nleaves, [&] (long i) {
    for (long j = 0; j < Leaves[i]->leftNeighbors.size(); j++) {
      node* L = Leaves[i];
      auto [u, v] = L->leftNeighbors[j];
      for (long k=0; k < Leaves[i]->n; k++)
        L->particles[k]->force = L->particles[k]->force + u->hold[v][k];
    }}, 1);

  // calculate forces within a node
  parlay::parallel_for (0, nleaves, [&] (long i) {self(Leaves[i]);});
}

// *************************************************************
// Calculates forces, placing them in particles[i].force
// *************************************************************
void forces(sequence<particle> &particles, double alpha) {
  parlay::internal::timer t("Time");
  get_global_transform()->precompute();

  // build the CK tree
  auto part_ptr = parlay::map(particles, [] (particle& p) {return &p;});
  node* a = build_tree(part_ptr, 0);
  t.next("build tree");

  // Sweep up the tree calculating multipole expansions for each node
  up_sweep(a);
  t.next("up sweep");

  // Determine all far-field interactions using the CK method
  interactions_count z = interactions(a);
  t.next("calculate far-field boxes");

  // Translate multipole to local expansions along the far-field
  // interactions
  do_indirect(a);
  t.next("apply far-field interactions");

  // Translate the local expansions down the tree to the leaves
  down_sweep(a);
  t.next("down sweep");

  // Add in all the direct (near field) interactions
  do_direct(a);
  t.next("apply near-field interactions");

  //std::cout << "Direct = " << (long) z.direct << " Indirect = " << z.indirect
  //    << " Boxes = " << numLeaves(a) << std::endl;
}
