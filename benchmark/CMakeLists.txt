# Benchmarks of example applications (word count, primes, etc.)
add_executable(bench_examples bench_examples.cpp)
target_link_libraries(bench_examples PRIVATE parlay benchmark_main)
target_compile_options(bench_examples PRIVATE -Wall -Wextra -Wfatal-errors)

