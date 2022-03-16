set(EXAMPLES "primes" "BFS" "word_counts" "tokens" "filter" "linefit"
  "mcss" "quickhull" "pagerank" "spanning_tree" "maximal_matching"
  "min_spanning_tree" "mergesort" "samplesort" "kth_smallest"
  "spectral_separator" "find_if" "rabin_karp" "kmeans_pp" "delaunay"
  "lasso_regression" "knn" "set_cover" "scan" "flatten" "reduce"
  "BFS_ligra" "triangle_count" "graph_color" "maximal_independent_set"
  "nbody_fmm" "cartesian_tree" "kcore" "cycle_count"
  "betweenness_centrality" "low_diameter_decomposition")

function(add_example NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE parlay)
endfunction()

foreach(example ${EXAMPLES})
  add_example(${example})
endforeach()

add_subdirectory(helper)
