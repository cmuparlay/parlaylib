#include <parlay/delayed.h>
#include <parlay/primitives.h>
#include <parlay/parallel.h>
#include <parlay/range.h>
#include <parlay/internal/get_time.h>

namespace delayed = parlay::delayed;

// **************************************************************
// A lightweight implementation of the Ligra interface
// Supports vertex_subset and edge_map.
// Implementation supports the forward sparse traversal, and backwards
// dense traversal.
// See:
//  Julian Shun, Guy E. Blelloch:
//  Ligra: a lightweight graph processing framework for shared memory.
//  PPOPP 2013:
// Interface for edge_map described below.
// **************************************************************

namespace ligra {

template<typename vertex>
struct vertex_subset {
  using sparse_t = parlay::sequence<vertex>;
  using dense_t = parlay::sequence<bool>;
  bool is_sparse;
  size_t n;
  size_t size() const {return n;}
  sparse_t sparse;
  dense_t dense;
  vertex_subset() : is_sparse(true), n(0) {}
  vertex_subset(sparse_t x) :
      sparse(std::move(x)), is_sparse(true), n(x.size()) {}
  vertex_subset(vertex v) :
      sparse(sparse_t(1,v)), is_sparse(true), n(1) {}
  vertex_subset(dense_t x) :
      dense(std::move(x)), is_sparse(false),
      n(parlay::count(x,true)) {}
  // must be vertices not already in set
  void add_vertices(const parlay::sequence<vertex>& V) {
    if (is_sparse) sparse = parlay::append(sparse, V);
    else for(auto v : V) dense[v] = true;
    n += V.size();
  }
  parlay::sequence<vertex> to_seq() {
    if (is_sparse) return sparse;
    else return parlay::pack_index<vertex>(dense);
  }
};

template <typename V>
struct identity {V operator() (V x) {return x;};};

// The constructor takes:
// G : the graph
// GT : the transposed graph
// fa : a function applied to each live edge returning a boolean
// cond : a function applied to the target returning a bool
// get : an optional argument that extracts a vertex from an edge
//       By default it is the identity function.
//       This can be used, for example, to add weights to edges.
//
// After constructing an edge_map, the function call operator can be
// applied to a vertex subset.  In such a call a (u,v) edge is
// "live" if u is in the vertex subset and if cond(v) returns true.
// fa(u,v) is applied to all live edges and a vertex v appears in
// the returned vertex subset if any fa(u,v) returns true.
//
// If get is not the default, then fa(u,v,e,backward) is
// applied to each live edge, where u is the source, v the target,
// e the edge (which might contain other info, such as a weight),
// and backward is a boolean flag indicating if the edge is a back
// edge from the transpose graph.
template<typename Graph, typename Fa, typename Cond,
    typename Get = identity<typename Graph::value_type::value_type>>
struct edge_map {
  using edge = typename Graph::value_type::value_type;
  using vertex = typename std::invoke_result<Get,edge>::type;
  using vertex_subset_ = vertex_subset<vertex>;
  using vertex_subset_sparse = parlay::sequence<vertex>;
  using vertex_subset_dense = parlay::sequence<bool>;
  long n;
  long m;
  Fa fa;
  Get get;
  Cond cond;
  const Graph& G;
  const Graph& GT;
  edge_map(Graph const &G, Graph const& GT, Fa fa, Cond cond, Get get = {}) :
      G(G), GT(GT), fa(fa), cond(cond), get(get), n(G.size()),
      m(parlay::reduce(parlay::delayed_map(G, parlay::size_of()))) {}

  bool f(vertex u, vertex v, edge e, bool backwards) {
    if constexpr (std::is_same<Get, identity<vertex>>::value)
      return fa(u,v);
    else return fa(u,v,e,backwards);
  }

  // The sparse version.  Checks all forward edges (u,v) from each
  // vertex u in vertices.  If cond(v) then fa(u,v) is applied and if
  // it returns true v is included in the returned vertex set.
  auto edge_map_sparse(vertex_subset_sparse const &vertices) {
    auto nested_pairs = parlay::map(vertices, [&] (vertex v) {
      return delayed::map(G[v], [=] (edge e) {
        return std::pair(v, e);});});
    auto pairs = delayed::flatten(nested_pairs);
    return delayed::to_sequence(delayed::map_maybe(pairs, [&] (auto p) {
      auto [u,e] = p;
      vertex v = get(e);
      return ((cond(v) && f(u,v,e,false))
              ? std::make_optional(v)
              : std::nullopt);}));
  }

  // The dense version.  Checks backwards from every vertex v for all
  // in edges (u,v).  If u is in vertices and cond(v) is satisfied,
  // fa(u,v) is applied, and if it returns true it is included in the
  // returned vertex set by setting a bool at location v.
  auto edge_map_dense(vertex_subset_dense const &vertices) {
    return parlay::tabulate(n, [&] (vertex v) {
      bool result = false;
      if (cond(v)) {
        parlay::find_if(GT[v], [&] (edge e) {
          if (!cond(v)) return true;
          vertex u = get(e);
          if (vertices[u] && f(u,v,e,true) && result == false)
            result = true;
          return false;});
      }
      return result;});
  }

  // Chooses between sparse and dense and changes representation
  // when swithching from one to the other.
  auto operator() (vertex_subset_ const &vertices) {
    auto l = vertices.size();
    bool do_dense;
    if (vertices.is_sparse) {
      auto d = parlay::reduce(parlay::delayed_map(vertices.sparse, [&] (long i) {
        return G[i].size();}));
      if ((l + d) > m/10) {
        parlay::sequence<bool> d_vertices(n, false);
        parlay::for_each(vertices.sparse, [&] (vertex i) {d_vertices[i] = true;});
        return vertex_subset_(edge_map_dense(d_vertices));
      } else return vertex_subset_(edge_map_sparse(vertices.sparse));
    } else {
      if (l > n/20) return vertex_subset_(edge_map_dense(vertices.dense));
      else return vertex_subset_(edge_map_sparse(parlay::pack_index<vertex>(vertices.dense)));
    }
  }
};
}
