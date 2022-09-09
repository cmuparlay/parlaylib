#include <random>
#include <limits>
#include <iostream>
#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/internal/get_time.h>
#include "helper/ligra_light.h"

#define SEED 15210
#define K_CAP 4

using namespace parlay;

// The types vertex and distance must be defined
constexpr auto max_distance = std::numeric_limits<distance>::max();
constexpr auto Empty = std::numeric_limits<vertex>::max();

class le_list{
public:
  sequence<sequence<std::pair<vertex,distance>>> A;
  sequence<std::atomic<int>> sizes;
  int capacity;

  le_list(vertex n) {
    capacity = K_CAP * ((int) std::floor(std::log2(n)) + 1);
    // create n empty le-lists, each of size capacity
    A = tabulate(n, [=] (vertex i) {
          return tabulate(capacity, [] (int j){
	    return std::make_pair((vertex) 0, max_distance);});});
    // initial sizes are 0
    sizes = tabulate<std::atomic<int>>(n, [] (vertex i) {return 0;});
  }

  //Inserts (j,dist) into i's LE-List
  void insert(vertex i, vertex j, distance dist){
    int ind = sizes[i]++;
    if(ind >= capacity) 
      std::cout << "Error: Exceeded list capacity. Try again with larger K_CAP value" << std::endl;
    A[i][ind] = std::make_pair(j,dist);
  }

  // Returns a sequence representation of le-lists. The order of the
  // lists is unspecified.
  sequence<sequence<std::pair<vertex,distance>>> pack() {
    return tabulate(sizes.size(), [&] (int i){
	     return to_sequence(A[i].head(sizes[i]));});
  }
};

struct vertex_info{
  std::atomic<vertex> root; // root of BFS search
  vertex root_ro; // read_only copy of root so not updated when written
  std::atomic<distance> step;   // step vertex was last updated
  vertex_info() : root(Empty), root_ro(Empty), step(0) {}
};

template <typename vertex, typename graph>
auto TruncatedBFS(graph& G, graph& GT,
		  sequence<vertex>& srcs,
		  sequence<distance>& delta_ro,
		  sequence<std::atomic<distance>>& delta,
		  le_list& L){
  parlay::internal::timer t;
  vertex n = G.size();
  auto order = tabulate(n, [&] (vertex i) { return Empty; });
  auto vtxs = sequence<struct vertex_info>(n);
  distance dist = 0;

  parallel_for(0, srcs.size(), [&] (vertex i){
    delta[srcs[i]] = 0;
    order[srcs[i]] = i;
    vtxs[srcs[i]].root = srcs[i];
    vtxs[srcs[i]].root_ro = srcs[i];
    vtxs[srcs[i]].step = 0;
    L.insert(srcs[i], srcs[i], 0);
  });
  
  // function to apply on each edge s->d
  auto edge_f = [&] (vertex s, vertex d) -> bool {
    if ((vtxs[d].root_ro == Empty
	 || order[vtxs[s].root_ro] < order[vtxs[d].root_ro])
	&& (dist < delta_ro[d])) {
      // if d is unvisited or root priority of d is larger than s,
      // and distance to d is greater than current distance,
      // then try to update the root of d
      auto less = [&] (vertex newv, vertex old) {
		    return (old == Empty || (order[newv] < order[old]));};

      if (write_min(&vtxs[d].root, vtxs[s].root_ro, less)) {
	L.insert(d, vtxs[s].root_ro, dist);
	// update nearest distance to d
	write_min(&delta[d], dist, std::less<distance>());
	// only true if first to update d (avoids duplicates in frontier)
	distance old = vtxs[d].step;
	return (dist > old) && vtxs[d].step.compare_exchange_strong(old, dist);
      }
      return false;
    }
    return false;
  };

  auto cond_f = [&] (vertex d) {
    vertex droot = vtxs[d].root;
    return dist < delta_ro[d] && (droot == Empty || order[droot] > 0);
  };

  auto frontier_map = ligra::edge_map(G, GT, edge_f, cond_f);
  auto frontier = ligra::vertex_subset(srcs);

  while (frontier.size() > 0) {
    dist++;
    frontier = frontier_map(frontier);

    // copy roots back to root_ro
    auto frontier_seq = frontier.to_seq();
    parallel_for(0, frontier_seq.size(), [&] (vertex v){
      vtxs[frontier_seq[v]].root_ro = vtxs[frontier_seq[v]].root.load();});
  }
}

template <class Graph>
auto create_le_list(Graph& G, Graph& GT) {
  parlay::random_generator gen(SEED);
  std::uniform_real_distribution<> dis(0.0,1.0);
  vertex n = G.size();
  parlay::internal::timer t("Time");

  // Psuedorandom priorities for the vertices
  auto R = tabulate(n, [&] (vertex i) { auto g = gen[i]; return dis(g); });

  // generate random permutation (P) and inverse (PInv) based on priorities
  auto verts = tabulate(n, [&] (vertex i) { return i;});
  auto P = stable_sort(verts, [&] (vertex u, vertex v) {return R[u] < R[v];});
  auto PInv = sequence<vertex>(n);
  parallel_for(0, n, [&] (vertex i){ PInv[P[i]] = i; });

  // empty le-lists
  le_list L(n);

  // initial distances of "infinity"
  auto delta = tabulate<std::atomic<distance>>(n, [] (vertex i) {
		     return max_distance;});
  
  // BFS using prefix doubling, i.e. increasing prefixes of double the size
  for(vertex r = 0; r < n; r = 2*r + 1){
    auto delta_ro = map(delta, [] (auto& d) {return d.load();});
    auto srcs = to_sequence(P.cut(r, std::min(2*r+1, n)));
    TruncatedBFS(G, GT, srcs, delta_ro, delta, L);
  }

  // pack out only the actual element in each le-list
  auto lists = L.pack();

  parallel_for(0, n, [&] (vertex i){
    //Sort lists in order of increasing vertex number (based on permutation)
    sort_inplace(lists[i], [&] (auto p1, auto p2) {
			     return PInv[p1.first] < PInv[p2.first];});

    //Prune lists for "extra elements"
    auto flags = tabulate(lists[i].size(), [&lists, i] (int j){
		   return j==0 || lists[i][j].second < lists[i][j-1].second;});
    lists[i] = pack(lists[i],flags);
  });
  return lists;
}
