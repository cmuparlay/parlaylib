#include <chrono>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "BFS.h"
#include "helper/ligra_light.h"

// **************************************************************
// This is a parallel implementation of Goldberg and Tarjan's
// push-relabel algorithm for max flow from:
//   Andrew V. Goldberg and Robert E. Tarjan.
//   A New Approach to the Maximum-Flow Problem.
//   JACM 1988.
//
// In particular we use the variant described in:
//   Niklas Baumstark, Guy E. Blelloch, and Julian Shun
//   Efficient Implementation of a Synchronous Parallel Push-Relabel Algorithm.
//   ESA 2015
//
// This variant uses global relabeling with bfs every once in a while
// and synchronous push-relabel rounds over all active vertices using
// shadow copies of the excess and label to avoid race conditions.
// No locks are required.
//
//**************************************************************

// the input format for graphs, i.e. adjacency sequence of neighbor-weight pairs
// if there is an edge from u to v, there must be one from v to u,
// although the capacities can differ or even be zero in one direction.
using vertex_id = int;
using weight = int;
using edges = parlay::sequence<std::pair<vertex_id,weight>>;
using weighted_graph = parlay::sequence<edges>;

struct max_flow {

  // internal representation used for edges and vertices
  struct edge {
    vertex_id v;
    int flow;
    int capacity;
    int partner_capacity; // makes bfs more efficient
    edge* partner; // to update flow in other direction
  };

  struct vertex {
    parlay::sequence<edge> edges;
    int label; // sometimes, perhaps more intuitively, called height
    int excess; // flow-in minus flow-out
    int current; // index of current edge
    int new_label; // shadow copy of label to avoid races
    std::atomic<int> new_excess; // used for concurrent pushes from neighbors
    parlay::sequence<vertex_id> pushes; // pushes to add to active list for next round
    std::atomic<bool> pushed; // used to check if already pushed onto active list
    vertex() : label(0), excess(0), current(0),
               new_excess(0), new_label(0), pushed(false) {}
  };

  parlay::sequence<vertex> vertices;
  parlay::sequence<vertex_id> active;
  int n, m;
  int s, t;
  std::chrono::duration<double> relabel_time;

  bool push(vertex_id ui) {
    vertex& u = vertices[ui];
    while (u.excess > 0 && u.current < u.edges.size()) {
      edge& e = u.edges[u.current];
      vertex& v = vertices[e.v];
      if (e.flow < e.capacity && u.label > v.label) {
        int release = std::min(e.capacity - e.flow, u.excess);
        if (release == e.capacity - e.flow) u.current++; // saturating
        e.flow += release;
        e.partner->flow -= release;
        u.excess -= release;
        v.new_excess += release; // atomic write with add
        push_active(u.pushes, e.v);
      } else u.current++;
    }
    if (u.excess > 0) push_active(u.pushes, ui);
    return u.current == u.edges.size(); // if true, need to relabel
  }

  void relabel(vertex_id ui) {
    vertex& u = vertices[ui];
    u.current = 0; // reset edge pointer to start
    int min_neighbor = reduce(delayed_map(u.edges, [&] (edge& e) {
                                return (e.flow < e.capacity) ? vertices[e.v].label : n;}),
                              parlay::minimum<int>());
    u.new_label = std::min(n, min_neighbor + 1);
  }

  void push_relabel() {
    // do push and relabel in parallel for each active vertex
    parlay::for_each(active, [&] (vertex_id ui) {
      vertex& u = vertices[ui];
      if (u.label < n && u.label > 0 && push(ui))
        relabel(ui);});

    // update principle copies of variables, returning new active vertices
    active = parlay::flatten(parlay::map(active, [&] (vertex_id ui) {
      vertex& u = vertices[ui];
      u.label = u.new_label;
      for (vertex_id vi : u.pushes) {
        vertex&v = vertices[vi];
        v.excess += v.new_excess;
        v.new_excess = 0;
        v.pushed = false;
      }
      return std::move(u.pushes);}));
  }

  int compute_max_flow(const weighted_graph& G, vertex_id source, vertex_id sink) {
    parlay::internal::timer tt("max flow");
    s = source;
    t = sink;
    initialize(G);
    tt.next("initialize graph");

    int cnt = 0;
    int rounds = 0;
    global_relabel(); // initial labeling
    auto last_time = current_time();
    while (active.size() > 0) {
      push_relabel();

      // Runs global_relabel when the push_relabels have taken three times
      // the time of the previous global_relabel.
      if (current_time() - last_time > 3 * relabel_time) {
        global_relabel();
        last_time = current_time();
        std::cout << "current flow: " << vertices[t].excess
                  << " num rounds: " << rounds << std::endl;
        rounds = 0;
      }
      rounds++;
    }
    tt.next("find max flow");
    check_correctness();
    return vertices[t].excess;
  }

  // pushes active for next round onto local queue
  void push_active(parlay::sequence<vertex_id>& a, vertex_id vi) {
    vertex& v = vertices[vi];
    bool flag = false;
    if (!v.pushed && v.pushed.compare_exchange_strong(flag,true))
      a.push_back(vi);
  }


  // This does a BFS to relabel based on distance in the residual graph
  // from the target.   Uses ligra for the BFS.
  void global_relabel() {
    auto start = current_time();
    parlay::internal::timer tt("global relabel", false);

    vertex_id cur_level = 0;
    //the distances from the target, initially all n, except target at 0
    auto d = parlay::tabulate<std::atomic<vertex_id>>(n, [&] (long i) {
      return (i==t) ? 0 : n; });

    // Need to generate a graph in the ligra format (sequence of sequences).
    auto G = parlay::delayed_map(vertices, [] (auto& vtx) {
      return vtx.edges;});

    // set up an edge map for the BFS
    auto edge_f = [&] (vertex_id u, vertex_id v, edge e, bool back) -> bool {
      vertex_id expected = n;
      bool saturated = ((back && (e.capacity == e.flow)) ||
                        (!back && (e.partner_capacity == -e.flow)));
      return (!saturated
              && d[v].compare_exchange_strong(expected, cur_level));};
    auto cond_f = [&] (vertex_id v) { return d[v] == n;};
    auto get_f = [] (edge e) { return e.v;};
    auto frontier_map = ligra::edge_map(G, G, edge_f, cond_f, get_f);

    // do the BFS
    auto frontier = ligra::vertex_subset(t);
    while (frontier.size() > 0) {
      cur_level++;
      frontier = frontier_map(frontier);
    }
    tt.next("BFS");

    parlay::parallel_for(0, n, [&] (int u) {
      vertices[u].current = 0;
      vertices[u].new_label = d[u];
      vertices[u].label = d[u];});

    //determine new active vertices
    active = parlay::filter(parlay::iota<vertex_id>(n), [&] (vertex_id vi) {
      vertex& v = vertices[vi];
      return v.label != 0 && v.label < n && v.excess > 0;});
    tt.next("Rest");
    relabel_time = current_time() - start;
  }

  void initialize(const weighted_graph& G) {
    parlay::internal::timer tt("initialize", false);
    n = G.size();
    m = parlay::reduce(parlay::map(G, parlay::size_of{}));

    // create augmented vertices and edges from graph
    vertices = parlay::sequence<vertex>(n);
    parlay::parallel_for(0, n, [&] (int u) {
      vertices[u].edges = parlay::tabulate(G[u].size(), [&] (int i) {
        auto [v, w] = G[u][i];
        return edge{v, 0, w, 0, nullptr};});});
    tt.next("create graph");

    // Cross link the edges
    auto x = parlay::flatten(parlay::tabulate(n, [&] (vertex_id u) {
      return parlay::delayed_map(vertices[u].edges, [&, u] (edge& e) {
        auto p = std::pair{std::min(u,e.v), std::max(u,e.v)};
        return std::pair{p, &e};});}));
    auto y = sort(std::move(x), [&] (auto a, auto b) {return a.first < b.first;});
    parlay::parallel_for(0, m/2, [&] (long i) {
      y[2*i].second->partner = y[2*i+1].second;
      y[2*i].second->partner_capacity = y[2*i+1].second->capacity;
      y[2*i+1].second->partner = y[2*i].second;
      y[2*i+1].second->partner_capacity = y[2*i].second->capacity;});
    tt.next("cross link");

    // initialize excess of source to "infinity"
    vertices[s].excess = std::numeric_limits<int>::max();
  }

  // checks at completion :
  //   o flow constraints (excess = flow-in - flow-out)
  //   o capacity constraints (no edge has more flow than capacity)
  //   o label constraints: label of neighbor in residual graph is at most one less
  //   o excess only if s or t or label = n
  //   o preservation of excess: ie., total equals original
  void check_correctness() {
    auto total = parlay::reduce(parlay::tabulate(n, [&] (int vi) {
      vertex& v = vertices[vi];
      long total_flow = parlay::reduce(parlay::map(v.edges, [] (edge e) {
        return e.flow;}));
      if (vi != s && total_flow != -v.excess) {
        std::cout << "flow does not match excess at " << vi << std::endl; abort();}
      long capacity_failed = parlay::reduce(parlay::map(v.edges, [] (edge e) {
        return (int) e.flow > e.capacity;}));
      if (capacity_failed > 0) {
        std::cout << "capacity oversubsribed from: " << vi << std::endl; abort();}
      long invalid_label = parlay::reduce(parlay::map(v.edges, [&] (edge e) {
        return (e.flow < e.capacity && v.label > vertices[e.v].label + 1);}));
      if (invalid_label > 0) {
        std::cout << "invalid label at: " << vi << std::endl; abort();}
      if (v.label != 0 && v.label < n && v.excess > 0) {
        std::cout << "left over excess at " << vi
                  << "excess = " << v.excess << "label = " << v.label << std::endl;
        abort();
      }
      return v.excess;}));

    int expected = std::numeric_limits<int>::max();
    if (total != expected)
      std::cout << "excess lost: " << expected-total << std::endl;
  }

  // yes, std clocks have ugly types
  auto current_time() -> std::chrono::time_point<std::chrono::system_clock>
  {return std::chrono::system_clock::now();}

};

int maximum_flow(const weighted_graph& G, int s, int t) {
  return max_flow().compute_max_flow(G, s, t);
}
