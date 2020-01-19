# -------------------------------------------------------------------
#                      Build system for ParlayLib
# -------------------------------------------------------------------
# Requirements:
#   - CMake version 3.13+
# -------------------------------------------------------------------

cmake_minimum_required(VERSION 3.13)
project(PARLAY VERSION 1.0
        DESCRIPTION "A collection of parallel algorithms and other support for parallelism in C++"
        LANGUAGES CXX)

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug Release RelWithDebInfo MinSizeRel)" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "--------------- General configuration -------------")
message(STATUS "CMake Generator:              ${CMAKE_GENERATOR}")
message(STATUS "Compiler:                     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type:                   ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS:              ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS        ${CMAKE_CXX_LINKER_FLAGS}")
message(STATUS "CMAKE_INSTALL_PREFIX:         ${CMAKE_INSTALL_PREFIX}" )

# Make sure -fno-omit-frame-pointer is set for profiling
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

# Set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# -------------------------------------------------------------------
#                       Library definition
             
add_library(parlay INTERFACE)
set(PARLAY_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
target_include_directories(parlay INTERFACE ${PARLAY_INCLUDE_DIR})
target_compile_features(parlay INTERFACE cxx_std_17)

# Link against system threads
find_package(Threads REQUIRED)
target_link_libraries(parlay INTERFACE Threads::Threads)

# GCC requires linking against libatomic
target_link_libraries(parlay INTERFACE atomic)

# -------------------------------------------------------------------
#                       Static analysis

message(STATUS "-------------------------- Static Analysis ------------------------")
add_subdirectory(analysis)

# -------------------------------------------------------------------
#                         Unit tests

message(STATUS "-------------------------- Unit Tests ------------------------")

# User option to build unit tests
option(BUILD_TESTS  "Build unit tests"       OFF)

if (BUILD_TESTS)
  # Set CMake options for GoogleTest
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

  # Download and configure GoogleTest
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         master
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
  message(STATUS "testing: Disabled (enable with -DBUILD_TESTS=On)")
endif()

# -------------------------------------------------------------------
#                       Benchmarks

