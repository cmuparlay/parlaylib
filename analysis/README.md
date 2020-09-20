# Static Analysis for ParlayLib

*This documentation is intended for developers/contributors of ParlayLib*

Parlay uses static analysis tools to help ensure code quality. Every commit to Parlay is currently
ran against [Cppcheck](http://cppcheck.sourceforge.net/), [Clang Tidy](https://clang.llvm.org/extra/clang-tidy/), and [include-what-you-use](https://include-what-you-use.org/) (IWYU). If you are contributing code to Parlay, it is strongly recommended that you install these tools locally so that you can run the checks before comitting your code and discovering errors after the fact.

## Installing the tools

You might be able to find Cppcheck, Clang Tidy, and IWYU in your favourite package manager. Be careful, though, as the version it contains might be out of date and incompatible with C++17. If so, you'll want to install the latest versions of the tools. The recommended way to install each of them is:

* [Cppcheck](http://cppcheck.sourceforge.net/) is easy to build from source.
* [Clang Tidy](https://clang.llvm.org/extra/clang-tidy/) can be installed on platforms that support Apt by adding the [LLVM Repositories](https://apt.llvm.org/), and then installing the package `clang-tidy-x`, where `x` is the version number. Alternatively, you can download pre-built binaries of the entire LLVM suite, or build  your own from source from [here](https://releases.llvm.org/download.html)
* [IWYU](https://include-what-you-use.org/) can be built from source by following the instructions [here](https://github.com/include-what-you-use/include-what-you-use/blob/master/README.md). You'll need to have a working installation of [LLVM](http://llvm.org/), which you can find from the same package repository as Clang Tidy, or by building it from source, as above

## Enabling static analysis targets in Parlay

The ParlayLib CMake project comes configured with optional build targets for running all of the static analysis tools for you. When you configure the CMake project, you can turn on each of the tools respectively with the additional options `-DENABLE_CPPCHECK=On`, `-DENABLE_CLANG_TIDY=On`, and `-DENABLE_IWYU=On`. Since the resulting targets do not actually build any executables, it does not matter whether this is done in a Debug or Release build. As a complete example, to configure an out-of-source build with all three tools enabled, you might write, from the repository root:

```
mkdir build && cd build
cmake -DENABLE_CPPCHECK=On -DENABLE_CLANG_TIDY=On -DENABLE_IWYU=On ..
```

CMake will automatically locate the appropriate executables for each of the tools, assuming that it can find them on your path. If CMake can not find the executables, you can manually set their locations by setting the flags `-DCPPCHECK_EXE=<path/to/cppcheck>`, `-DCLANG_TIDY_EXE=<path/to/clang-tidy>`, or `-DIWYU_EXE=<path/to/iwyu>`.
## Running the static analysis targets

Once you have enabled the static analysis tools, the project will contain custom build targets for each of them, and one for running all of them. The targets created are:

* `cppcheck` / `cppcheck-all`: Runs Cppcheck on each library header file
* `clang-tidy` / `clang-tidy-all`: Runs Clang Tidy on each library header file
* `iwyu` / `iwy-all`: Runs IWYU on each library header file
* `analysis-all`: Runs all enabled static analysis tools on each library header file

To invoke these targets, either use `make <target>` (e.g. `make cppcheck`) if your build is configured using make (the default for CMake on Linux), or in general, `cmake --build . --target <target>`, which will invoke your configured build system.

## Adding new library files

If you add new files to Parlay, you will need to add them to the list of files to be analysed in *analysis/CMakeLists.txt*.
