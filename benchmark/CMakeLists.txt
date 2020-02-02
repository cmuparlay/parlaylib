
# Benchmarks of standard library algorithm equivalents
add_executable(bench_stl_algs bench_stl_algs.cpp)
target_link_libraries(bench_stl_algs PRIVATE parlay benchmark_main)
target_compile_options(bench_stl_algs PRIVATE -Wall -Wextra -Wfatal-errors)

# Benchmarks of non-standard additional algorithms (e.g. integer sort)
add_executable(bench_additional_algs bench_additional_algs.cpp)
target_link_libraries(bench_additional_algs PRIVATE parlay benchmark_main)
target_compile_options(bench_additional_algs PRIVATE -Wall -Wextra -Wfatal-errors)

# Benchmarks of example applications (word count, primes, etc.)
add_executable(bench_examples bench_examples.cpp)
target_link_libraries(bench_examples PRIVATE parlay benchmark_main)
target_compile_options(bench_examples PRIVATE -Wall -Wextra -Wfatal-errors)

