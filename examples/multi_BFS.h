#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

#include "helper/ligra_light.h"

struct node_info {                 // On return
  std::atomic<ulong> visited;      // S_r^0 (from Akiba, Iwata, Yoshina paper)
  std::atomic<ulong> visited_prev; // S_r^{-1}
  std::atomic<char> d;             // P
  node_info() : visited(0), visited_prev(0), d(0) {}
};

// Find shortest paths to all vertices from a center vertex and 63 of its neighbors
// Picks the vertex with highest degree as the center, and its 63 neighbors with
// highest degree.   Aborts if the highest degree is less than 63.
// Currently for undirected graph.
template <typename vertex, typename graph>
auto multi_BFS(vertex start, const graph& G) {
  // set to true to turn timing on
  parlay::internal::timer t("bfs time", false);
  int round = 0;
  auto sizes = parlay::map(G, parlay::size_of());
  t.next("map");

  // find index of max degree vertex, which will be the center vertex
  vertex i = parlay::max_element(sizes) - sizes.begin();
  t.next("max");
  auto ngh = G[i];
  if (ngh.size() < 63) abort();

  // sort neighbors of center by degree (max first)
  auto sub_sizes = parlay::map(ngh, [&] (vertex j) {return std::make_pair(G[j].size(), j);});
  sub_sizes = parlay::sort(sub_sizes, [&] (auto a, auto b) {return a.first > b.first;});

  // take center vertex + 63 neighbors with highest degree
  auto vertices = parlay::tabulate<vertex>(64, [&] (long j) {
                      if (j == 0) return i;
                      else return sub_sizes[j-1].second;});
  t.next("ngh");

  // initialize node structure
  parlay::sequence<node_info> nodes(G.size());
  // set i-th bit (from bottom) for i-th vertex
  // note that bit 0 is for the center
  for (int i = 0; i < 64; i++) 
    nodes[vertices[i]].visited = 1ul << i;
  t.next("init");

  // apply to each edge traversed
  // read from visited in u and write to visited_prev in v
  auto edge_f = [&] (vertex u, vertex v) -> bool {
    ulong u_visited = nodes[u].visited.load();
    ulong v_visited = nodes[v].visited_prev.load();

    // can skip if v has visited all nodes u has
    if ((u_visited | v_visited) != v_visited) {

      // add if visited in u but not v
      nodes[v].visited_prev.fetch_or(u_visited);

      // ensure that v is only added to the frontier once
      char old_d = nodes[v].d.load();
      return (old_d < round &&
	      nodes[v].d.compare_exchange_strong(old_d, round));
    } else return false; };

  // only apply an edge if this is true on the target
  auto cond_f = [&] (vertex v) {
    // no need to update v if already visited from the center
    return !(nodes[v].visited.load() & 1);};

  auto frontier_map = ligra::edge_map(G, G, edge_f, cond_f);

  // initialize frontier with the center vertex and 63 neighbors
  auto frontier = ligra::vertex_subset<vertex>();
  frontier.add_vertices(vertices);
  t.next("head");

  long total = 0;
  while (frontier.size() > 0) {
    round++;
    //long m = frontier.size();
    //total += m;
    //std::cout << "frontier size: " << std::dec <<  m << ", " << total << std::endl;

    // map from frontier over edges
    frontier = frontier_map(frontier, false);
    t.next("map");

    // update all vertices on frontier
    frontier.apply([&] (vertex v) {
      // swap visited and visited_prev if the center vertex appears in visited_prev
      if ((nodes[v].visited_prev.load() & 1) == 1) {
        ulong tmp = nodes[v].visited_prev.load();
        nodes[v].visited_prev = nodes[v].visited.load();
        nodes[v].visited = tmp;
        // otherwise just copy from visited_prev to visited
      } else nodes[v].visited = nodes[v].visited_prev.load();});
    t.next("update");
  }

  return nodes;
}
