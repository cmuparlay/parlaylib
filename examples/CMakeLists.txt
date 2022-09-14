set(EXAMPLES "primes" "BFS" "word_counts" "tokens" "filter" "linefit"
  "mcss" "quickhull" "pagerank" "spanning_tree" "maximal_matching"
  "kruskal" "mergesort" "samplesort" "kth_smallest"
  "spectral_separator" "find_if" "rabin_karp" "kmeans_pp" "delaunay"
  "lasso_regression" "knn" "set_cover" "scan" "flatten" "reduce"
  "BFS_ligra" "triangle_count" "graph_color" "maximal_independent_set"
  "nbody_fmm" "cartesian_tree" "kcore" "cycle_count"
  "betweenness_centrality" "low_diameter_decomposition" "quicksort" "filter_kruskal"
  "knuth_shuffle" "minimum_edit_distance" "bigint_add" "bellman_ford" "suffix_array"
  "le_list" "longest_common_prefix" "longest_repeated_substring" "range_min"
  "fast_fourier_transform" "bucketed_dijkstra" "push_relabel_max_flow" 
  "knuth_morris_pratt" "huffman_tree" "decision_tree_c45" "karatsuba" "suffix_tree"
  "2d_linear_program" "box_kdtree" "radix_tree" "ray_trace" "hash_map" "oct_tree"
  "3d_range" "rectangle_intersection")

function(add_example NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE parlay)
endfunction()

foreach(example ${EXAMPLES})
  add_example(${example})
endforeach()

