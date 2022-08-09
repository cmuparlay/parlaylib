
# Including and Configuring

ParlayLib is a lightweight header-only library, so it is easy to integrate into your new or existing projects. There are many ways to accomplish this. Choose whichever works best for your project.


- [Approaches for including Parlay](#approaches-for-including-parlay)
  * [Installing globally and including via CMake](#installing-globally-and-including-via-cmake)
  * [Using CMake's FetchContent](#using-cmakes-fetchcontent)
  * [Including manually or with another build system](#including-manually-or-with-another-build-system)
  * [Copying the code directly into your project](#copying-the-code-directly-into-your-project)
- [Configuring parallel execution](#configuring-parallel-execution)
  * [Using Parlay with Cilk, OpenMP, or TBB](#using-parlay-with-cilk-openmp-or-tbb)
  * [Setting the number of worker threads](#setting-the-number-of-worker-threads)

## Approaches for including Parlay

### Installing globally and including via CMake

Parlay comes configured to work with CMake, so if CMake is your preferred choice of build system, integrating Parlay is straightforward. To install Parlay, create a build directory (any name is fine) in the Parlay root directory, and from that build directory, run the following

```
cmake ..
cmake --build . --target install
```

The default installation path is usually `/usr/local/`. If you'd like to install it somewhere else, add the `DCMAKE_INSTALL_PREFIX` option like so:

```
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/your/custom/installation/path
cmake --build . --target install
```

You may need to run the installation step with `sudo` depending on the chosen installation path. Now that Parlay is installed, you can locate and include it via CMake by adding the following to the CMakeLists.txt of your project

```
find_package(Threads REQUIRED)
find_package(Parlay REQUIRED)
```

If your chosen installation path is not one of the defaults, you may need to aid CMake in locating it by setting the flag [CMAKE_PREFIX_PATH](https://cmake.org/cmake/help/latest/variable/CMAKE_PREFIX_PATH.html). You can then link Parlay to any of your CMake targets by writing

```
target_link_libraries(my_target <PRIVATE|PUBLIC|INTERFACE> Parlay::parlay)
```


### Using CMake's FetchContent

Rather than installing Parlay globally, if you are using CMake, you can configure your build to download and link to its own internal copy of Parlay using the `FetchContent` module. To do so, add the following to your `CMakeLists.txt`:

```
include(FetchContent)
FetchContent_Declare(parlaylib
  GIT_REPOSITORY  https://github.com/cmuparlay/parlaylib.git
  GIT_TAG         master
)
FetchContent_GetProperties(parlaylib)
if(NOT parlaylib_POPULATED)
  FetchContent_Populate(parlaylib)  
  add_subdirectory(${parlaylib_SOURCE_DIR} EXCLUDE_FROM_ALL)
endif()
```

You can replace `master` with a particular commit hash, tag, or branch if you need a specific version of the code. Then, to link Parlay with your target, just write

```
target_link_libraries(my_target <PRIVATE|PUBLIC|INTERFACE> parlay)
```


### Including manually or with another build system

If you use a different build system, you have the option of installing Parlay via CMake (see above), or installing it manually, and then including it manually or via your preferred build system. If you install Parlay to a location that is not in your compiler's include path, you should tell your compiler where to find it by adding an include flag (or your build system's equivalent) like so.

```
-I/path/to/parlay/include/
```

To ensure that Parlay builds correctly, make sure that you are compiling with C++17 (or better) enabled by adding `-std=c++17` (or later) or your compiler's equivalent (e.g. `/std:c++17` on Microsoft) to your compiler options. You'll also need threading support, which is usually achieved via one of the flags `-pthread`, `-pthreads`, or your operating system's equivalent.

### Copying the code directly into your project

If fancy build systems are not your thing, the tried and tested way to include Parlay in your project is to simply copy the source code of Parlay directly into your own project. Since the library is header only, this should work out of the box, assuming you add the required flags (C++17 and threading support, see above). One possible way to do this while still enabling updates to ParlayLib is to include it as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) of your project's Git repository.

## Configuring parallel execution

### Using Parlay with Cilk, OpenMP, or TBB

If you're already using CilkPlus, OpenCilk, OpenMP, or Thread Building Blocks (TBB), and just want to use Parlay's algorithms without its parallel scheduler, that is easy to do. When building your program, simply add the appropriate compile definition given in the table below.

Parlay will then use the specified framework's parallel operations to support its algorithms instead of its own scheduler. Note that you are still responsible for ensuring that Cilk/OMP/TBB are enabled/linked against your program, just as you normally would without Parlay.

Library | Definition to enable | Other flags | Notes
---|---|---|---
CilkPlus | `-DPARLAY_CILKPLUS` | `-fcilkplus` | Requires GCC 7 (removed in later versions)
OpenCilk | `-DPARLAY_OPENCILK` | `-fopencilk` | Requires the [OpenCilk compiler](https://github.com/OpenCilk/opencilk-project/releases)
OpenMP | `-DPARLAY_OPENMP` | `-fopenmp` / `/openmp` | Requires an OpenMP runtime compatible with your compiler
TBB | `-DPARLAY_TBB` | - | Requires linking against the TBB library

### Setting the number of worker threads

The number of worker threads used by Parlay can be controled by setting the environment variable `PARLAY_NUM_THREADS`, assuming you are using Parlay's default scheduler. If you are using Cilk or OpenMP, you will need to use their equivalents instead (`CILK_NWORKERS` for Cilk and `OMP_NUM_THREADS` for OpenMP). TBB does not provide an environment variable, but you can control the number of threads using the [task arena interface](https://www.intel.com/content/www/us/en/develop/documentation/onetbb-documentation/top/onetbb-developer-guide/parallelizing-data-flow-and-dependence-graphs/flow-graph-tips-and-tricks/flow-graph-tips-for-limiting-resource-consumption/attach-flow-graph-to-an-arbitrary-task-arena/guiding-task-scheduler-execution.html).

