#include <parlay/delayed.h>
#include <parlay/primitives.h>
#include <parlay/parallel.h>
#include <parlay/range.h>
#include <parlay/internal/get_time.h>

namespace delayed = parlay::delayed;

// **************************************************************
// A lightweight implementation of the Ligra interface
// supports vertex_subset and edge_map.
// See:
//  Julian Shun, Guy E. Blelloch:
//  Ligra: a lightweight graph processing framework for shared memory.
//  PPOPP 2013:
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

template<typename Graph, typename Fa, typename Cond> 
struct edge_map {
  using vertex = typename Graph::value_type::value_type;
  using vertex_subset_ = vertex_subset<vertex>;
  using vertex_subset_sparse = parlay::sequence<vertex>;
  using vertex_subset_dense = parlay::sequence<bool>;
  long n;
  long m;
  Fa fa;
  Cond cond;
  const Graph& G;
  const Graph& GT;
  edge_map(Graph const &G, Graph const& GT, Fa fa, Cond cond) :
    G(G), GT(GT), fa(fa), cond(cond), n(G.size()),
    m(parlay::reduce(parlay::delayed_map(G, parlay::size_of()))) {}

  // The sparse version.  Checks all forward edges (u,v) from each
  // vertex u in vertices.  If cond(v) then fa(u,v) is applied and if
  // it returns true v is included in the returned vertex set.
  auto edge_map_sparse(vertex_subset_sparse const &vertices) {
    auto nested_edges = parlay::map(vertices, [&] (vertex v) {
	return parlay::delayed_tabulate(G[v].size(), [&, v] (long i) {
	    return std::pair(v, G[v][i]);});});
    auto edges = delayed::flatten(nested_edges);
    return delayed::to_sequence(delayed::map_maybe(edges, [&] (auto e) {
	  auto [u,v] = e;
	  return (cond(v) && fa(u, v)) ? std::make_optional(v) : std::nullopt;}));
  }

  // The dense version.  Checks backwards from every vertex v for all
  // in edges (u,v).  If u is in vertices and cond(v) is satisfied,
  // fa(u,v) is applied, and if it returns true it is included in the
  // returned vertex set by setting a bool at location v.
  auto edge_map_dense(vertex_subset_dense const &vertices) {
    return parlay::tabulate(n, [&] (vertex v) {
	bool result = false;
	if (cond(v)) {
	  parlay::find_if(GT[v], [&] (vertex u) {
	      if (!cond(v)) return true;
	      if (vertices[u] && fa(u,v) && result == false) result = true;
	      return false;});
	}
	return result;});
  }

  // Chooses between sparse and dense and changes representation
  // when swithching from one to the other.
  auto operator() (vertex_subset_ const &vertices) {
    auto l = vertices.size();
    parlay::internal::timer t;
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
