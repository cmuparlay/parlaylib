#include <atomic>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/io.h>

#include "helper/ligra_light.h"

struct node_info {
  std::atomic<ulong> visited;
  std::atomic<ulong> visited_prev;
  std::atomic<char> d;
  node_info() : visited(0), visited_prev(0), d(0) {}
};
  
template <typename vertex, typename graph>
auto multi_BFS(vertex start, const graph& G) {
  parlay::internal::timer t;
  int round = 0;
  auto sizes = parlay::map(G, parlay::size_of());
  t.next("map");
  
  vertex i = parlay::max_element(sizes) - sizes.begin();
  t.next("max");
  auto ngh = G[i];
  if (ngh.size() < 63) abort();
  auto sub_sizes = parlay::map(ngh, [&] (vertex j) {return std::make_pair(G[j].size(), j);});
  sub_sizes = parlay::sort(sub_sizes, [&] (auto a, auto b) {return a.first > b.first;});
  
  auto vertices = parlay::tabulate<vertex>(64, [&] (long j) {
						 if (j == 0) return i;
						 else return sub_sizes[j-1].second;});
  t.next("ngh");
  
  parlay::sequence<node_info> nodes(G.size());
  for (int i = 0; i < 64; i++) 
    nodes[vertices[i]].visited = 1ul << i;
  t.next("init");
  
  auto edge_f = [&] (vertex u, vertex v) -> bool {
		  ulong u_visited = nodes[u].visited.load();
		  ulong v_visited = nodes[v].visited_prev.load();
		  if ((u_visited | v_visited) != v_visited) {
		    nodes[v].visited_prev.fetch_or(u_visited);
		    char old_d = nodes[v].d.load();
		    return (old_d < round &&
			    nodes[v].d.compare_exchange_strong(old_d, round));
		  } else return false; };

  auto cond_f = [&] (vertex v) {
		  return !(nodes[v].visited.load() & 1);};

  auto frontier_map = ligra::edge_map(G, G, edge_f, cond_f);
  auto frontier = ligra::vertex_subset<vertex>();
  frontier.add_vertices(vertices);
  t.next("head");

  long total = 0;
  while (frontier.size() > 0) {
    round++;
    long m = frontier.size();
    total += m;
    std::cout << "frontier size: " << std::dec <<  m << ", " << total << std::endl;
      frontier = frontier_map(frontier);
      t.next("map");
      frontier.apply([&] (vertex v) {
	if ((nodes[v].visited_prev.load() & 1) == 0) {
	  ulong tmp = nodes[v].visited_prev.load();
	  nodes[v].visited_prev = nodes[v].visited.load();
	  nodes[v].visited = tmp;
	} else nodes[v].visited = nodes[v].visited_prev.load();});
      t.next("update");
  }

  return nodes;
}
