# Unit tests for ParlayLib

*This documentation is intended for developers/contributors of ParlayLib*

Parlay is accompanied by a set of unit tests intended to assert the correctness of its data structures and primitives, and to mitigate the risk of introducing bugs during updates. If you are contributing code to Parlay, you should add appropriate unit tests to ensure that your code is tested, and update any existing unit tests that are affected by your changes. Always remember to run the tests locally before comitting changes.

## Configuring the build for testing

To configure the CMake project to run the tests, you'll need to add the flag `-DPARLAY_TEST=On`. You should also ensure that the build is in *Debug* mode, by adding the flag `-DCMAKE_BUILD_TYPE=Debug`. In order to maintain a separation between your test environment and benchmark environment, it is a good habbit to initialize them as separate CMake builds in different directories. Creating a Debug build with tests enabled can be achieved with these minimal commands from the repository root.

```
mkdir -p build/Debug && cd build/Debug
cmake -DCMAKE_BUILD_TYPE=Debug -DPARLAY_TEST=On ../..
```

By default, CMake will use your system's default C++ compiler. To use a specific compiler, you can either set the environment variable `CXX=<your/favourite/compiler>`, or set the CMake option `-DCMAKE_CXX_COMPILER=<your/favourite/compiler>`.

## Building the tests

Once the build is configured, you can build the tests either by running `make` if the build is configured to use make, which is the default, or in general, by running `cmake --build .`. If using make, you can build the tests in parallel by adding the `-j` flag to the `make` command.

## Running the tests

The compiled tests are located in the *test* subdirectory of your configured build directory. Tests can be ran individually from here. Alternatively, running the target `test` (i.e. running `make test` or `cmake --build . --target test`), or executing the `ctest` command will run all of the tests.

## Building and running sanitizer-instrumented tests

All of the unit tests can also be compiled with [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html) (ASAN), [UndefinedBehaviourSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html) (UBSAN), and [MemorySanitizer](https://clang.llvm.org/docs/MemorySanitizer.html) (MSAN). Enabling ASAN and UBSAN is easy as long as you have a compiler that supports their full functionality. [Clang](https://clang.llvm.org/) is recommended, since GCC does not support the full functionality of these tools.

To enable ASAN and UBSAN, add, to your CMake build configurations, the flags `-DBUILD_ASAN_TESTS=On`, and `-DBUILD_UBSAN_TESTS=On` respectively. With these flags enabled, each unit test will also be compiled to an additional target with the respective sanitizers enabled. Running the `test` target as above will run both the instrumented and non-instrumented tests. Alternatively, to run a specific subset of the tests, we also provide the targets
* `check`: Run only the non-instrumented tests
* `check-asan`: Run only the tests instrumented with ASAN
* `check-ubsan`: Run only the tests instrumented with UBSAN
* `check-msan`: Run only the tests instrumented with MSAN

Enabling MSAN is a little bit more complicated, since, unlike ASAN and UBSAN, MSAN only works when all linked code, including the standard library, is instrumented. This means that you need a separately compiled copy of the standard library that has already been instrumented with MSAN. Some instructions on how to do this are provided [here](https://github.com/google/sanitizers/wiki/MemorySanitizerLibcxxHowTo). Once you have built an MSAN-instrumented libc++, you should provide its location to the CMake build configuration via `-DLIBCXX_MSAN_PATH=<path/to/instrumented/libcxx>`. Finally, the unit tests with MSAN-instrumentation can enabled by adding to the CMake build configuration, the flag `-DBUILD_MSAN_TESTS=On`.

## Running memcheck tests

Tests can also be ran with memcheck ([Valgrind](https://valgrind.org/)) by adding the flag `-DENABLE_MEMCHECK_TESTS=On` to the CMake build configuration. This will add additional test targets which can be ran with the entire suite of tests, or separately by invoking the `check-memcheck` target.

## Adding new tests

All of the tests in the suite are written using the [Google Test](https://github.com/google/googletest) framework. To add a new test set, write a new test cpp file in the *<project_root>/test* directory in the Google Test format. Then, register it in *<project_root>/test/CMakeLists.txt* with the line

```
add_dtests(NAME <your_test_name> FILES <your_test_cpp_files> LIBS parlay)
```

If necessary, multiple cpp files can be listed after the FILES option. You can also specify additional libraries that need to be linked with the test, if necessary, by adding them after the LIBS option.
