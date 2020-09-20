# Benchmarks for ParlayLib

*This documentation is intended for developers/contributors of ParlayLib*

Since Parlay is a library designed for high-performance computing, we maintain a set of benchmarks to keep an eye of the speed of the core primitives and underlying implementations. If you contribute to Parlay, you should write benchmarks for any new primitives that you add. Additionally, any changes to code that affects the core primitives should be benchmarked to ensure that it does not lead to performance degradation. The benchmarks are designed to be ran on machines with a high number of processors, ideally 20+, but the more the better.

## Configuring the build for benchmarking

To configure the CMake project to run the benchmarks, you'll need to add the flag `-DPARLAY_BENCHMARK=On`. You should also ensure that the build is in *Release* mode, by adding the flag `-DCMAKE_BUILD_TYPE=Release`. In order to maintain a separation between your test environment and benchmark environment, it is a good habbit to initialize them as separate CMake builds in different directories. Creating a Release build with benchmarks enabled can be achieved with these minimal commands from the repository root.

```
mkdir -p build/Release && cd build/Release
cmake -DCMAKE_BUILD_TYPE=Release -DPARLAY_BENCHMARK=On ../..
```

By default, CMake will use your system's default C++ compiler. To use a specific compiler, you can either set the environment variable `CXX=<your/favourite/compiler>`, or set the CMake option `-DCMAKE_CXX_COMPILER=<your/favourite/compiler>`.

## Building the benchmarks

Once the build is configured, you can build the benchmarks either by running `make` if the build is configured to use make, which is the default, or in general, by running `cmake --build .`. If using make, you can build the benchmarks in parallel by adding the `-j` flag to the `make` command.

## Running the benchmarks

The compiled benchmarks are located in the *benchmark* subdirectory of your configured build directory. You can set the number of worker threads used by Parlay by setting the `PARLAY_NUM_THREADS` environment variable. By default, it uses the number of hardware threads on the machine, which is usually good. In order to get the best performance, some additional steps should be noted:

* Performance is improved significantly by using [jemalloc](http://jemalloc.net/). You should install jemalloc and preload it by setting the environment variable `LD_PRELOAD=<path/to/jemalloc>`.
* If you're using a machine with [NUMA](https://en.wikipedia.org/wiki/Non-uniform_memory_access) memory, you should set the allocation strategy to interleave all, by prepending the executable with `numactl -i all`
* To smooth out the variance in the timings, the benchmarks can be repeated and averaged by adding the `--benchmark_repetitions=<num_repetitions>` flag to the benchmark executable. To stop the benchmark framework from displaying all of the runs and only show the averages, also add the `--benchmark_display_aggregates_only=true` flag.

A complete example command for executing a benchmark with good settings is given below.

```
LD_PRELOAD=/usr/local/lib/libjemalloc.so numactl -i all ./benchmark/bench_standard --benchmark_repetitions=20
```

## Adding new benchmarks

All of the benchmarks are written using the [Google Benchmark](https://github.com/google/benchmark) framework. To add a new benchmark set, write a new benchmark cpp file in the *<project_root>/benchmark* directory in the Google Benchmark format. Then, register it in *<project_root>/benchmark/CMakeLists.txt* by following the same structure as the existing benchmarks.