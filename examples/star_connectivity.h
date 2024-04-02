#include <utility>
#include <random>
#include <optional>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/random.h>

using rg = typename parlay::random_generator;
std::uniform_int_distribution<int> dis(0,1);
std::uniform_real_distribution<double> rdis(0.0,1.0);

// **************************************************************
// Graph Connectivity using Star Contraction
// The graph is given by a sequence of edges, and the number of vertices
// Returns a label for each vertex, and the number of components
// **************************************************************

// **************************************************************
// The recursive helper function
//   E : the remaining edges
//   V : the remaining vertices
//   parents : a mapping from all original vertices to their parent
//   generator: for random coin flipping
// **************************************************************
template <typename vertex>
parlay::sequence<vertex>
star_contract(const parlay::sequence<std::pair<vertex,vertex>>& E,
              parlay::sequence<vertex> V,
              parlay::sequence<vertex>& parents,
              rg generator) {
  using edge = std::pair<vertex, vertex>;

  // std::cout << "size : " << E.size() << ", " << V.size() << std::endl;
    
  if (E.size() == 0) return V;

  auto heads = [&] (vertex u) {rg r = generator[u]; return dis(r);};
    
  // hook tails into heads
  for_each(E, [&] (const edge e) {
      auto [u,v] = e;
      if (heads(u) && !heads(v) && parents[v] == v) parents[v] = u;
      else if (heads(v) && !heads(u) && parents[u] == u) parents[u] = v;});

  // filter away all hooked tails
  auto V_new = filter(V, [&] (vertex v) {return parents[v] == v;});
  
  // update edges, while removing self edges
  auto E_new = map_maybe(E, [&] (const edge e) {
      auto [u,v] = e;
      edge en = edge(parents[u], parents[v]);
      return (en.first == en.second) ? std::optional<edge>() : std::optional<edge>(en);});
  
  // recurse on contracted graph
  auto roots = star_contract(E_new, std::move(V_new), parents, generator[parents.size()]);

  // update tails to new head index
  for_each(V, [&] (const vertex& v) {
      parents[v] = parents[parents[v]];});
  return roots;
}

// **************************************************************
// The main function, calls helper function
// **************************************************************
template <typename vertex>
std::pair<parlay::sequence<vertex>, parlay::sequence<vertex>>
star_connectivity_simple(const parlay::sequence<std::pair<vertex,vertex>>& E,
                         long n) {
  using edge = std::pair<vertex, vertex>;
  auto parents = parlay::tabulate(n, [] (vertex i) {return i;});
  auto roots = star_contract(E, parents, parents, rg(0));
  return std::pair(parents, roots);
}

// **************************************************************
// The following is an optimized version that samples the edges to reduce the work
// Includeds both a modified helper function, and a revised main function
// **************************************************************

template <typename vertex>
parlay::sequence<vertex>
star_contract_sample(const parlay::sequence<std::pair<vertex,vertex>>& E,
                     parlay::sequence<vertex> V,
                     parlay::sequence<vertex>& parents,
                     rg generator) {
  using edge = std::pair<vertex, vertex>;

  // std::cout << "size : " << E.size() << ", " << V.size() << std::endl;
    
  if (E.size() == 0) return V;

  // This is the only difference from the simple version (star_contract)
  // If graph is too dense, samples the edges.
  if (E.size() > 5 * V.size()) {
    float frac = V.size() * 3.0 / E.size();
    rg r = generator[0];
    auto Ee = parlay::map_maybe(parlay::iota(E.size()), [&] (long i) {
        rg r = generator[i];
        return rdis(r) < frac ? std::optional<edge>(E[i]) : std::optional<edge>();});
    return star_contract_sample(Ee, V, parents, generator[E.size()]);
  }

  auto heads = [&] (vertex u) {rg r = generator[u]; return dis(r);};
    
  // hook tails into heads
  for_each(E, [&] (const edge e) {
      auto [u,v] = e;
      if (heads(u) && !heads(v) && parents[v] == v) parents[v] = u;
      else if (heads(v) && !heads(u) && parents[u] == u) parents[u] = v;});

  // filter away all hooked tails
  auto V_new = filter(V, [&] (vertex v) {return parents[v] == v;});
  
  // update edges, while removing self edges
  auto E_new = map_maybe(E, [&] (const edge e) {
      auto [u,v] = e;
      edge en = edge(parents[u], parents[v]);
      return (en.first == en.second) ? std::optional<edge>() : std::optional<edge>(en);});
  
  // recurse on contracted graph
  auto roots = star_contract_sample(E_new, std::move(V_new), parents, generator[parents.size()]);

  // update tails to new head index
  for_each(V, [&] (const vertex& v) {
      parents[v] = parents[parents[v]];});
  return roots;
}

template <typename vertex>
std::pair<parlay::sequence<vertex>, parlay::sequence<vertex>>
star_connectivity(const parlay::sequence<std::pair<vertex,vertex>>& E,
                  long n) {
  using edge = std::pair<vertex, vertex>;
  auto parents = parlay::tabulate(n, [] (vertex i) {return i;});

  // sample based version
  auto roots = star_contract_sample(E, parents, parents, rg(0));

  // only keep edges that go between components
  auto E_new = map_maybe(E, [&] (const edge e) {
      auto [u,v] = e;
      edge en = edge(parents[u], parents[v]);
      return (en.first == en.second) ? std::optional<edge>() : std::optional<edge>(en);});

  // now run non sampled based version and update parents
  roots = star_contract(E_new, roots, parents, rg(0));
  for_each(parlay::iota(n), [&] (const vertex& v) {
      parents[v] = parents[parents[v]];});
  
  return std::pair(parents, roots);
}
