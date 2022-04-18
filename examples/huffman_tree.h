#include <tuple>
#include <algorithm>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/delayed.h>

// **************************************************************
// Builds a huffman tree in parallel from a sequence of probabilities
// Sorts the probabilities and then pairs up all probabilities from
// the minimum value to twice that value.  Repeats until done.
// Returns a pointer to all leaves (for encoding), and to the root
// (for decoding).
// **************************************************************

// the tree type
struct node { bool is_leaf; node* parent;};

struct leaf : node {
  long idx;
  leaf(long i) : node{true, nullptr}, idx(i) {}
};

struct interior : node {
  node* L; node* R;
  interior(node* L, node* R) : node{false, nullptr}, L(L), R(R) {}
};

parlay::type_allocator<leaf> leaf_pool;
parlay::type_allocator<interior> interior_pool;

auto huffman_tree(const parlay::sequence<float>& probs) {
  // create the leaves for each probability
  // this is returned as way to traverse from leaf to root on encoding
  auto leaves = parlay::tabulate(probs.size(), [&] (auto i) -> node* {
    return leaf_pool.allocate(i);});

  // zip probabilites with leaves and sort into reverse order based on probabilities
  auto greater = [] (auto a, auto b) {return std::get<0>(a) > std::get<0>(b);};
  auto active = sort(parlay::delayed::zip(probs, leaves), greater);
  auto abegin = active.begin();

  // find earliest entry in active up to top that is greater or equal
  // to prob
  auto search = [&] (float prob, long top) {
    auto x = std::lower_bound(abegin, abegin + top, prob,
                              [] (auto a, float p) {return std::get<0>(a) > p;});
    return x - abegin;};

  long top = active.size();

  // we can pairwise merge all probabilities up to twice the min probability
  float cutoff = 2 * std::get<0>(active[top-1]);
  long mid = search(cutoff, top);
  while (top > 1) {
    cutoff *= 2;
    // ensure top - mid is a multiple of 2
    if ((top - mid) % 2 == 1) mid++;

    // pair up nodes from mid to top
    auto pairs = parlay::tabulate((top-mid)/2, [&] (long i) {
      auto [lprob, lnode] = active[mid + 2*i];
      auto [rprob, rnode] = active[mid + 2*i + 1];
      float new_prob = lprob + rprob;
      node* new_node = interior_pool.allocate(lnode, rnode);
      lnode->parent = rnode->parent = new_node;
      return std::tuple<float,node*>{new_prob, new_node};});

    // find new cutoff
    long bot = search(cutoff, mid);

    // merge new nodes with region from bot to mid based on probabilities
    auto merged = parlay::merge(pairs, active.cut(bot,mid), greater);

    // update top and mid, and copy merged result back into active
    top = bot + merged.size();
    mid = bot;
    parlay::copy(merged, active.cut(mid, top));
  }
  return std::tuple{leaves, std::get<1>(active[0])};
}

void delete_tree(node* T, int depth=0) { // delete tree in parallel
  if (T->is_leaf)
    leaf_pool.retire(static_cast<leaf*>(T));
  else {
    interior* TI = static_cast<interior*>(T);
    parlay::par_do_if(depth < 16,
                      [&] {delete_tree(TI->L, depth+1);},
                      [&] {delete_tree(TI->R, depth+1);});
    interior_pool.retire(TI);
  }
}
