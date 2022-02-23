set(EXAMPLES "primes" "BFS" "word_counts" "tokens" "filter" "linefit" "mcss"
  "quickhull" "pagerank" "spanning_tree" "maximal_matching"
  "min_spanning_tree" "mergesort" "samplesort" "kth_smallest"
  "spectral_separator" "find_if" "rabin_karp" "kmeans_pp" "delaunay" "lasso"
  "knn" "set_cover" "scan" "flatten")

function(add_example NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE parlay)
endfunction()

foreach(example ${EXAMPLES})
  add_example(${example})
endforeach()
