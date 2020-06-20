include(dtest)

# ------------------------------ Range and Slice -------------------------------

add_dtests(NAME test_range FILES test_range.cpp LIBS parlay)
add_dtests(NAME test_slice FILES test_slice.cpp LIBS parlay)
add_dtests(NAME test_delayed_sequence FILES test_delayed_sequence.cpp LIBS parlay)
add_dtests(NAME test_sequence FILES test_sequence.cpp LIBS parlay)

# ----------------------------- Parlay Scheduling ------------------------------

add_dtests(NAME test_scheduler FILES test_scheduler.cpp LIBS parlay)
add_dtests(NAME test_parlay_plugin FILES test_parlay_scheduler_plugin.cpp LIBS parlay)
add_dtests(NAME test_seq_scheduling FILES test_seq_scheduling.cpp LIBS parlay)

# --------------------- External scheduler integration tests -------------------

# Cilk integration test (requires a Cilk-compatible compiler)
set(CILK_FLAGS "-fcilkplus")
option(TEST_CILK "Enable unit test for Cilk integration (requires a Cilk-compatible compiler)" Off)
if(TEST_CILK)
  flags_supported(${CILK_FLAGS} CILK_SUPPORT)
  if(CILK_SUPPORT)
    add_dtests(NAME test_cilk_plugin FILES test_cilk_plugin.cpp LIBS parlay FLAGS ${CILK_FLAGS})
  else()
    message(FATAL_ERROR "You are trying to build the Cilk integration tests, but your compiler does not support Cilk. You'll need to switch to a compiler that does, or turn off the Cilk tests (-DTEST_CILK=Off)")
  endif()
endif()

# TBB integration test (requires TBB installed)
option(TEST_TBB "Enable unit test for TBB integration (requires TBB to be installed and on the library/include path" Off)
if (TEST_TBB)
  add_dtests(NAME test_tbb_plugin FILES test_tbb_plugin.cpp LIBS parlay tbb)
endif()

# OMP integration test (requires OMP installed)
option(TEST_OMP "Enable unit test for OpenMP integration (requires a compatible installation of OpenMP for your compiler" Off)
if (TEST_OMP)
  add_dtests(NAME test_omp_plugin FILES test_omp_plugin.cpp LIBS parlay FLAGS -fopenmp)
endif()
