# Parlaylib Examples

A collection of parallel algorithms based on parlaylib.

Each example contains a .h file which contains the implementation, and
a .cpp file which has some test code to run it.

The algorithms were designed to be concise but fast.  Many are close
to the fastest (if not the fastest) codes available for multicore
machines with 10+ cores.  Many are as simple as their sequential
counterparts, and sometimes simpler.

Every example uses parlay sequences and some subset of the primitives.
Many examples use delayed sequences, and many use the parallel random
number generation (especially for testing).  The most common
primitives used are tabulate, map, reduce, scan, filter, and flatten.
Other common primitives include sort, find_if, and the
{group,reduce}_by_{key,index} primitives.

The examples are as follows, broken down into categories.

## Graph algorithms

- bellman_ford
- betweenness_centrality
- BFS
- BFS_ligra
- bucketed_dijkstra
- filter_kruskal
- graph_color
- kcore
- kruskal
- low_diameter_decomposition
- maximal_independent_set
- maximal_matching
- pagerank
- push_relabel_max_flow
- set_cover
- spanning_tree
- spectral_separator
- triangle_count

## Numerical
- bigint_add
- fast_fourier_transform
- karatsuba
- linefit
- mcss
- primes
- range_min

## Strings
- cartesian_tree
- huffman_tree
- knuth_morris_pratt
- longest_common_prefix
- longest_repeated_substring
- minimum_edit_distance
- rabin_karp
- suffix_array
- tokens
- word_counts

## Machine Learning etc.

- decision_tree_c45
- kmeans_pp
- lasso_regression

## Geometry and Graphics

- delaunay
- knn
- quickhull
- nbody_fmm

## Sorting etc.

- kth_smallest
- mergesort
- quicksort
- samplesort

## Basic operations

- filter
- find_if
- flatten
- reduce
- scan
- knuth_shuffle
- cycle_count
