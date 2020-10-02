# Benchmarks of example applications (word count, primes, etc.)
add_executable(bench_examples bench_examples.cpp)
target_link_libraries(bench_examples PRIVATE parlay benchmark_main)
target_compile_options(bench_examples PRIVATE -Wall -Wextra -Wfatal-errors)

# Benchmarks of standard primitives with good performance coverage
add_executable(bench_standard bench_standard.cpp)
target_link_libraries(bench_standard PRIVATE parlay benchmark_main)
target_compile_options(bench_standard PRIVATE -Wall -Wextra -Wfatal-errors)

# Benchmarks of standard primitives with good performance coverage
add_executable(bench_parsing bench_parsing.cpp)
target_link_libraries(bench_parsing PRIVATE parlay benchmark_main)
target_compile_options(bench_parsing PRIVATE -Wall -Wextra -Wfatal-errors)
