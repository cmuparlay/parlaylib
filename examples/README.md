# Parlaylib Examples

A collection of parallel algorithms based on ParlayLib.

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

- bellman_ford : single source shortest paths supporting negative edge weights
- betweenness_centrality : a BFS implementation of betweenness centrality
- BFS : breadth first search from a given source
- BFS_ligra : same, but using the ligra interface that supports forward/backwards optimization
- bucketed_dijkstra : a version of dijkstra's algorithm for single source shortest paths that uses a bucketed priority queue
- filter_kruskal : a version of Kruskal's minimum-spanning-forest algorithm that avoids sorting all edges
- graph_color : graph coloring based on the greedy degree heuristic
- kcore : returns the "coreness" of every vertex of a graph.
- kruskal : Parallel Kruskal's minimum-spanning-forest algorithm.
- le_list : Least elements lists for vertices of a graph.
- low_diameter_decomposition : returns a decomposition of a graph such that beta*m edges cross between components, and each component has radius log(n)/beta.
- maximal_independent_set : greedy (lexicographically first) MIS on random permutation of vertices
- maximal_matching : greedy (lexicographically first) maximal matching on random permutation of edges
- pagerank : Page and Brin's algorithm for ranking pages in the web graph
- push_relabel_max_flow : Maximum flow using the Goldberg-Tarjan push-relabel method
- set_cover : approximate set cover with (1 + epsilon) (ln n) approximation guarantee
- spanning_tree : union-find implementation of spanning forest
- spectral_separator : graph separator based on second-least eigenvector of Laplacian
- triangle_count : count the number of triangles in an undirected graph

## Numerical
- bigint_add : add (and subtract) two arbitrary precision numbers
- fast_fourier_transform : Cooley-Tukey algorithm for the FFT
- karatsuba : n^{log_2 3}-work algorithm for multiplying arbitrary precision numbers
- linefit : fits a set of points in 2d to a line
- mcss : maximum-contiguous-subset-sum algorithm using divide-and-conquer (reduce)
- primes : returns the primes up to n.
- range_min : data structure for reporting minimum over any range of values in a sequence

## Strings
- cartesian_tree : Tree over an ordered sequence in which root of each subtree has highest priority
- huffman_tree : Build a Huffman tree from a sequence of probabilities
- knuth_morris_pratt : string search using the KMP algorithm
- longest_common_prefix : from a suffix sorted array, generated the LCP between each adjacent pair
- longest_repeated_substring : find the LRS in a string
- minimum_edit_distance : report the minimum number of inserts and deletes to covert one string to another
- rabin_karp : string search using the Rabin-Karp algorithm
- radix_tree : a trie with such that vertices with a single child are contracted out
- suffix_array : for a string generate array of indices sorted by the suffix starting at that index
- suffix_tree : for a string generate its suffix tree
- tokens : given a string, break into a sequence of tokens given separator characters
- word_counts : given a string, count the number of occurrences of each token in the string

## Machine Learning etc.

- decision_tree_c45 : Quinlan's C4.5 algorithm for building a decision tree
- kmeans_pp : Arthur and Vassilvitskii's kmeans++ algorithm for finding kmeans
- lasso_regression : Coordinate descent algorithm for LASSO regression

## Geometry and Graphics

- 2d_linear_program : solve linear program max c^Tx, Ax <= b, in 2 dimensions.
- delaunay : Delaunay triangulation in 2 dimensions
- oct_tree : the z-tree variant of oct trees in any constant dimension
- knn : k-nearest-neighbors of a set of points in constant dimension
- quickhull : convex hull of points in 2 dimensions using quickhull algorithm
- nbody_fmm : n-body force calculations based on Callahan-Kosaraju's variant of the fast-multipole-method.
- box_kdtree : builds a kd_tree over a set of 3d boxes using the surface area heuristic (SAH)
- ray_trace : traces a set of rays to first triangle they intersect (also called ray-casting)
- 3d_range : for a set of points reports all points within a radius r.
- rectangle_intersection : for a set of rectangles in 3d reports all pairs that intersect

## Sorting etc.

- kth_smallest : randomized algorithm to report the kth smallest element of an unsorted sequence
- mergesort : merge sort
- quicksort : quick sort
- samplesort : a sample-based sort (can be though of as quicksort with many pivots).  This is the fastest of the sorts.

## Basic operations

- filter : possible implementation of parlaylib's filter
- find_if : possible implementation of parlaylib's find_if
- flatten : possible implementation of parlaylib's flatten
- reduce : possible implementation of parlaylib's reduce
- scan : possible implementation of parlaylib's scan
- knuth_shuffle : possible implementation of parlaylib's random_shuffle
- cycle_count : counts number of cycles in a permutation
- hash_map : concurrent hash based map supporting insert, find, remove
