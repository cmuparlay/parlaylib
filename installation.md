
# Installation

ParlayLib is a lightweight header-only library, so it is easy to integrate into your new or existing projects. There are many ways to accomplish this.

### Installing and including via CMake

Parlay comes configured to work with CMake, so if CMake is your preferred choice of build system, integrating Parlay is straightforward. To install Parlay, create a build directory (any name is fine) in the Parlay root directory, and from that build directory, configure CMake by running `cmake ..`. If you wish to change the installation location (the default is usually /usr/local/) then you can add the option `-DCMAKE_INSTALL_PREFIX:PATH=/your/installation/path`. After configuring, run `cmake --build . --target install` (or just `make install` for short if you happen to be using Make as the underlying build tool), possibly with `sudo` if necessary, to install Parlay.

Now that Parlay is installed, you can locate and include it via CMake by adding the following to the CMakeLists.txt of your project

```
find_package(Threads REQUIRED)
find_package(Parlay CONFIG REQUIRED)
```

You can then link Parlay to any of your targets by writing

```
target_link_libraries(my_target PRIVATE Parlay::parlay)
```

You are now ready to go!

### Installing and including manually

If you use a different build system, you have the option of installing Parlay via CMake (see above), or installing it manually, and then including it manually or via your preferred build system. If you install Parlay to a location that is not in your compiler's include path, you should tell your compiler where to find it by adding an include flag like so.

```
-I/path/to/parlay/include/location
```

To ensure that Parlay builds correctly, make sure that you are compiling with C++17 enabled by adding `-std=c++17` or equivalent (e.g. `/std:c++17` on Microsoft) to your compiler options. You'll also need threading support, which is usually achieved via one of the flags `-pthread`, `-pthreads`, or your operating system's equivalent.

### The old fashioned way

If fancy build systems are not your thing, the tried and true way to include Parlay in your project is to simply copy the source code of Parlay directly into your own project. Since the library is header only, this should work out of the box, assuming you add any required flags (see above). One possible way to do this while still enabling updates to ParlayLib is to include it as a [Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) of your project's Git repository.

## Using Parlay with Cilk, OpenMP, or TBB

If you're already using Cilk, OpenMP, or Thread Building Blocks, and just want to use Parlay's algorithms without its parallel scheduler, that is easy to do. When building your program, simply add the appropriate compile definition as below.

```
-DPARLAY_CILKPLUS
-DPARLAY_OPENCILK
-DPARLAY_OPENMP
-DPARLAY_TBB
```

Parlay will then use the specified framework's parallel operations to support its algorithms instead of its own scheduler. Note that you are still responsible for ensuring that Cilk/OMP/TBB are enabled/linked against your program, just as you normally would without Parlay.
