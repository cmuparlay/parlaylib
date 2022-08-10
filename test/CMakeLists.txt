include(dtest)

# ------------------------------ Range and Slice -------------------------------

add_dtests(NAME test_range FILES test_range.cpp LIBS parlay)
add_dtests(NAME test_slice FILES test_slice.cpp LIBS parlay)

# ------------------------------ Data Structures -------------------------------

add_dtests(NAME test_delayed_sequence FILES test_delayed_sequence.cpp LIBS parlay)
add_dtests(NAME test_sequence FILES test_sequence.cpp LIBS parlay)
add_dtests(NAME test_hash_table FILES test_hash_table.cpp LIBS parlay)

# ------------------------------ Delayed sequences -------------------------------

add_dtests(NAME test_delayed_map FILES test_delayed_map.cpp LIBS parlay)
add_dtests(NAME test_delayed_reduce FILES test_delayed_reduce.cpp LIBS parlay)
add_dtests(NAME test_delayed_scan FILES test_delayed_scan.cpp LIBS parlay)
add_dtests(NAME test_delayed_filter FILES test_delayed_filter.cpp LIBS parlay)
add_dtests(NAME test_delayed_filter_op FILES test_delayed_filter_op.cpp LIBS parlay)
add_dtests(NAME test_delayed_flatten FILES test_delayed_flatten.cpp LIBS parlay)
add_dtests(NAME test_delayed_for_each FILES test_delayed_for_each.cpp LIBS parlay)
add_dtests(NAME test_delayed_zip FILES test_delayed_zip.cpp LIBS parlay)

# ----------------------------- Sorting Algorithms ------------------------------

add_dtests(NAME test_merge_sort FILES test_merge_sort.cpp LIBS parlay)
add_dtests(NAME test_quicksort FILES test_quicksort.cpp LIBS parlay)
add_dtests(NAME test_bucket_sort FILES test_bucket_sort.cpp LIBS parlay)
add_dtests(NAME test_integer_sort FILES test_integer_sort.cpp LIBS parlay)
add_dtests(NAME test_counting_sort FILES test_counting_sort.cpp LIBS parlay)
add_dtests(NAME test_sample_sort FILES test_sample_sort.cpp LIBS parlay)

# -------------------------------- Primitives ---------------------------------

add_dtests(NAME test_primitives FILES test_primitives.cpp LIBS parlay)
add_dtests(NAME test_sorting_primitives FILES test_sorting_primitives.cpp LIBS parlay)
add_dtests(NAME test_random FILES test_random.cpp LIBS parlay)
add_dtests(NAME test_group_by FILES test_group_by.cpp LIBS parlay)
add_dtests(NAME test_monoid FILES test_monoid.cpp LIBS parlay)
add_dtests(NAME test_transpose FILES test_transpose.cpp LIBS parlay)

# -------------------------------- Concurrency ----------------------------------

add_dtests(NAME test_hazptr_stack FILES test_hazptr_stack.cpp LIBS parlay)

# -------------------------- Uninitialized memory testing ---------------------------

add_dtests(NAME test_uninitialized_memory FILES test_uninitialized_memory.cpp LIBS parlay NO_SANITIZE)

# -------------------------------- IO ---------------------------------

add_dtests(NAME test_io FILES test_io.cpp LIBS parlay)
add_dtests(NAME test_file_map FILES test_file_map.cpp LIBS parlay)
add_dtests(NAME test_file_map_fallback FILES test_file_map.cpp LIBS parlay FLAGS "-DPARLAY_USE_FALLBACK_FILE_MAP")

# --------------------------- Parsing and Formatting ----------------------------

if(NOT WSL)
  add_dtests(NAME test_parsing FILES test_parsing.cpp LIBS parlay)
endif()

add_dtests(NAME test_formatting FILES test_formatting.cpp LIBS parlay)

# ----------------------------- Parlay Scheduling ------------------------------

add_dtests(NAME test_parallel FILES test_parallel.cpp LIBS parlay)

# ----------------------------- Parlay Allocator ------------------------------

add_dtests(NAME test_allocator FILES test_allocator.cpp LIBS parlay)

# ----------------------------- Utilities ------------------------------

add_dtests(NAME test_relocate FILES test_relocate.cpp LIBS parlay)
add_dtests(NAME test_padded FILES test_padded.cpp LIBS parlay)
