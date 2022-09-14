#include <iostream>
#include <string>
#include <random>

#include "parlay/primitives.h"
#include "parlay/random.h"
#include "parlay/io.h"

#include "box_kdtree.h"

// **************************************************************
// Driver
// **************************************************************

int main(int argc, char* argv[]) {
  auto usage = "Usage: box_kdtree <n>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    long n;
    int k = 10;
    try { n = std::stol(argv[1]); }
    catch (...) { std::cout << usage << std::endl; return 1; }
    parlay::random_generator gen(0);
    std::uniform_real_distribution<float> dis(0.0,100.0);

    // generate n random points in a unit square
    auto boxes = parlay::tabulate(n, [&] (long i) {
      auto r = gen[i];
      float x = dis(r), y = dis(r), z = dis(r);
      return Bounding_Box{range{x,x+1},range{y,y+1},range{z,z+1}};
    });
    tree_node* r;
    long num_leaves;
    parlay::internal::timer t("Time");
    for (int i=0; i < 5; i++) {
      t.start();
      r = kdtree_from_boxes(boxes);
      t.next("box_kdtree");
      num_leaves = r->n;
      tree_node::node_allocator.retire(r);
    }

    std::cout << "Number of boxes across the leaves = " << num_leaves << std::endl;
  }
}

