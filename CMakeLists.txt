# -------------------------------------------------------------------
#                      Build system for ParlayLib
# -------------------------------------------------------------------
# Requirements:
#   - CMake version 3.14+
# -------------------------------------------------------------------

cmake_minimum_required(VERSION 3.14)
project(PARLAY VERSION 2.2.1
        DESCRIPTION "A collection of parallel algorithms and other support for parallelism in C++"
        LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug Release RelWithDebInfo MinSizeRel)" FORCE)
  message(STATUS "No build type specified. Defaulted to DEBUG.")
  message(STATUS "To specify a build type, add -DCMAKE_BUILD_TYPE=<DEBUG/RELEASE/RELWITHDEBINFO/MINSIZEREL>")
endif(NOT CMAKE_BUILD_TYPE)

# Make sure -fno-omit-frame-pointer is set for profiling
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
else()
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oy-")
endif()

message(STATUS "PARLAY VERSION ${PARLAY_VERSION}")
message(STATUS "---------------------------- General configuration -----------------------------")
message(STATUS "CMake Generator:                ${CMAKE_GENERATOR}")
message(STATUS "Compiler:                       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:                     ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS:                ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:          ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:        ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS          ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_INSTALL_PREFIX:           ${CMAKE_INSTALL_PREFIX}" )

# Set module path
list(APPEND CMAKE_MODULE_PATH "${PARLAY_SOURCE_DIR}/cmake")

# -------------------------------------------------------------------
#                       Library definition

add_library(parlay INTERFACE)
set(PARLAY_INCLUDE_DIR "${PARLAY_SOURCE_DIR}/include")
target_include_directories(parlay INTERFACE
  $<BUILD_INTERFACE:${PARLAY_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

option(PARLAY_USE_CXX_20 "Require Parlay to build with C++20")
if (PARLAY_USE_CXX_20)
  target_compile_features(parlay INTERFACE cxx_std_20)
else()
  target_compile_features(parlay INTERFACE cxx_std_17)
endif()

# Link against system threads
find_package(Threads REQUIRED)
target_link_libraries(parlay INTERFACE Threads::Threads)


message(STATUS "-------------------------------- Library options ------------------------------")

# -------------------------------------------------------------------
#                Enable/disable elastic parallelism

option(PARLAY_ELASTIC_PARALLELISM "Enable elastic parallelism" On)

# -------------------------------------------------------------------
#              Support for alternative parallel runtimes

option(PARLAY_CILKPLUS "Enable integration with CilkPlus")
option(PARLAY_OPENCILK "Enable integration with OpenCilk")
option(PARLAY_OPENMP "Enable integration with OpenMP")
option(PARLAY_TBB "Enable integration with TBB")
option(PARLAY_SEQUENTIAL "Run Parlay with no parallelism")

if (PARLAY_CILKPLUS)
  message(STATUS "Parlay CilkPlus integration enabled")
  check_cxx_compiler_flag("-fcilkplus" CILKPLUS_SUPPORT)
  if(CILKPLUS_SUPPORT)
    target_compile_definitions(parlay INTERFACE PARLAY_CILKPLUS)
    target_compile_options(parlay INTERFACE "-fcilkplus")
    target_link_options(parlay INTERFACE "-fcilkplus")
  else()
    message(FATAL_ERROR "You are trying to enable CilkPlus integration, but your compiler does not support CilkPlus.
                         You'll need to switch to a compiler that does, or turn it off (-DPARLAY_CILKPLUS=Off)")
  endif()

elseif(PARLAY_OPENCILK)
  message(STATUS "Parlay OpenCilk integration enabled")
  check_cxx_compiler_flag("-fopencilk" OPENCILK_SUPPORT)
  if(OPENCILK_SUPPORT)
    target_compile_definitions(parlay INTERFACE PARLAY_OPENCILK)
    target_compile_options(parlay INTERFACE "-fopencilk")
    target_link_options(parlay INTERFACE "-fopencilk")
  else()
    message(FATAL_ERROR "You are trying to enable OpenCilk integration, but your compiler does not support OpenCilk.
                         You'll need to switch to a compiler that does, or turn it off (-PARLAY_OPENCILK=Off)")
  endif()
elseif(PARLAY_OPENMP)
  message(STATUS "Parlay OpenMP integration enabled")
  find_package(OpenMP)
  if(OpenMP_CXX_FOUND)
    target_compile_definitions(parlay INTERFACE PARLAY_OPENMP)
    target_link_libraries(parlay INTERFACE OpenMP::OpenMP_CXX)
  else()
    message(FATAL_ERROR "You are trying to enable OpenMP integration, but your compiler does not support OpenMP, or
                         maybe you don't have OpenMP installed. Fix this, or turn it off (-PARLAY_OPENMP=Off)")
  endif()
elseif(PARLAY_TBB)
  message(STATUS "Parlay TBB integration enabled")
  find_package(TBB)
  if(TBB_FOUND)
    target_compile_definitions(parlay INTERFACE PARLAY_TBB)
    target_link_libraries(parlay INTERFACE TBB::tbb)
  else()
    message(FATAL_ERROR "You are trying to enable TBB integration, but the TBB library could not be found. Install it,
                         point CMake towards where you have it installed, or turn it off (-PARLAY_TBB=Off)")
  endif()
elseif(PARLAY_SEQUENTIAL)
  message(STATUS "Parlay sequential mode enabled (no parallelism!!)")
  target_compile_definitions(parlay INTERFACE PARLAY_SEQUENTIAL)
else()
  message(STATUS "Using Parlay scheduler. Switch with -DPARLAY_{CILKPLUS,OPENCILK,OPENMP,TBB}=On")
  if (PARLAY_ELASTIC_PARALLELISM)
    message(STATUS "Elastic parallelism enabled. Disable with -DPARLAY_ELASTIC_PARALLELISM=Off")
    target_compile_definitions(parlay INTERFACE PARLAY_ELASTIC_PARALLELISM=true)
    
    # On Windows we need to link against synchronization.lib for WaitOnAddress
    if (WIN32)
      target_link_libraries(parlay INTERFACE synchronization)
    endif()
  else()
    message(STATUS "Elastic parallelism disabled. Enable with -DPARLAY_ELASTIC_PARALLELISM=On")
    target_compile_definitions(parlay INTERFACE PARLAY_ELASTIC_PARALLELISM=false)
  endif()
endif()


# -------------------------------------------------------------------
#                         Installation

install(TARGETS parlay
        EXPORT parlay_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        
include(CMakePackageConfigHelpers)
write_basic_package_version_file("ParlayConfigVersion.cmake"
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY SameMajorVersion)
                                 
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/ParlayConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/ParlayConfig.cmake"
  INSTALL_DESTINATION
  ${CMAKE_INSTALL_DATAROOTDIR}/parlay/cmake)

install(EXPORT parlay_Targets
        FILE ParlayTargets.cmake
        NAMESPACE Parlay::
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/parlay/cmake)
        
install(FILES "${PROJECT_BINARY_DIR}/ParlayConfig.cmake"
              "${PROJECT_BINARY_DIR}/ParlayConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/parlay/cmake)
        
install(DIRECTORY ${PARLAY_INCLUDE_DIR}/parlay DESTINATION include)

# -------------------------------------------------------------------
#                       Static analysis

message(STATUS "------------------------------- Static Analysis --------------------------------")
add_subdirectory(analysis)

# -------------------------------------------------------------------
#                         Unit tests

message(STATUS "---------------------------------- Unit Tests ----------------------------------")

# User option to build unit tests
option(PARLAY_TEST  "Build unit tests"       OFF)

if (PARLAY_TEST)
  # Set CMake options for GoogleTest
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

  # Download and configure GoogleTest
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         main
  )
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    message(STATUS "testing: Configuring GoogleTest")
    FetchContent_Populate(googletest)  
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
    add_subdirectory(${googletest_SOURCE_DIR}
                     ${googletest_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()

  # Include test targets
  message(STATUS "testing: Enabled")
  include(CTest)
  add_subdirectory(test)

else()
  message(STATUS "testing: Disabled (enable with -DPARLAY_TEST=On)")
endif()

# -------------------------------------------------------------------
#                       Benchmarks

message(STATUS "---------------------------------- Benchmarks ----------------------------------")

# User option to build benchmarks
option(PARLAY_BENCHMARK    "Build microbenchmarks"  OFF)

if (PARLAY_BENCHMARK)
  # Benchmark should not run its own unit tests
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

  # Download Benchmark library
  include(FetchContent)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY  https://github.com/google/benchmark.git
    GIT_TAG         v1.7.1
  )
  FetchContent_GetProperties(benchmark)
  if(NOT benchmark_POPULATED)
    message(STATUS "benchmarks: Configuring Google Benchmark")
    FetchContent_Populate(benchmark)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "")
    add_subdirectory(${benchmark_SOURCE_DIR}
                     ${benchmark_BINARY_DIR}
                     EXCLUDE_FROM_ALL)
  endif()

  # Include benchmark targets
  message(STATUS "benchmarks: Enabled")
  add_subdirectory(benchmark)

else()
  message(STATUS "benchmarks: Disabled (enable with -DPARLAY_BENCHMARK=On)")
endif()

# -------------------------------------------------------------------
#                          Examples

message(STATUS "----------------------------------- Examples -----------------------------------")

# User option to build examples
option(PARLAY_EXAMPLES    "Build examples"  OFF)

if (PARLAY_EXAMPLES)
  message(STATUS "examples: Enabled")
  add_subdirectory(examples)
else()
  message(STATUS "examples: Disabled (enable with -DPARLAY_EXAMPLES=On)")
endif()

option(PARLAY_EXAMPLE_DATA  "Download example data"  OFF)

if (PARLAY_EXAMPLE_DATA)
  message(STATUS "example data: On")
  include(FetchContent)
  FetchContent_Declare(
    exampleData
    URL http://www.cs.cmu.edu/~parlay/data.tar.gz
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/data
  )
  FetchContent_MakeAvailable(exampleData)
else()
  message(STATUS "example data: Off (add -DPARLAY_EXAMPLE_DATA=On to download)")
endif()
