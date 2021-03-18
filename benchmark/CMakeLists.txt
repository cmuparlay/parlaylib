# Benchmarks for Parlay
#
# Benchmarks are implemented using Google Benchmark.
#
set(NUMA_COMMAND numactl -i all)

function(add_benchmark NAME)
  add_executable(bench_${NAME} bench_${NAME}.cpp)
  target_link_libraries(bench_${NAME} PRIVATE parlay benchmark_main)
  target_compile_options(bench_${NAME} PRIVATE -Wall -Wextra -Wfatal-errors -march=native)

  if(PARLAY_BENCHMARK_NUMACTL_TARGETS)
    add_custom_target(numactl_bench_${NAME}
      COMMAND ${NUMA_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/bench_${NAME} --benchmark_counters_tabular=true
    )
    add_dependencies(numactl_bench_${NAME} bench_${NAME})
  endif()
endfunction()

add_benchmark(examples)
add_benchmark(standard)
add_benchmark(parsing)
add_benchmark(sequence)
