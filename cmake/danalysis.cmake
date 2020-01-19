# -------------------------------------------------------------------
#           DAnalysis: Dan's static analysis framework
#
# - Supports cppcheck, clang-tidy, infer, and include-what-you-use
# - Creates targets for individual files and for analyzing all files
#
# -------------------------------------------------------------------

# User options to enable static analysis tools
option(CPPCHECK "Enable cppcheck static analysis" OFF)
option(CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(INFER "Enable infer static analysis" OFF)
option(IWYU "Enable include-what-you-use analysis" OFF)

macro(CONFIGURE_DANALYSIS)
  set(options OPTIONAL "")
  set(oneValueArgs "")
  set(multiValueArgs INCLUDES FILES)
  cmake_parse_arguments(CONFIGURE_DANALYSIS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  set(STATIC_ANALYSIS_TARGETS "")

  # ---------------------------- cppcheck -----------------------------

  if(CPPCHECK)
    # Find cppcheck installation
    find_program(
      CPPCHECK_EXE
      NAMES "cppcheck"
      DOC "Path to cppcheck executable"
    )

    if(NOT CPPCHECK_EXE)
      message(FATAL_ERROR "cppcheck enabled but could not find cppcheck executable")
    else()
      message(STATUS "cppcheck:                     Enabled (${CPPCHECK_EXE})")

      # Args for cppcheck. Enable all levels of errors
      set(CPPCHECK_ARGS
        "--enable=warning,performance,style,portability,missingInclude"
        "--suppress=missingIncludeSystem"
        "-I${CONFIGURE_DANALYSIS_INCLUDES}"
        "--error-exitcode=1"
        "--inline-suppr"
        "--suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck-suppress"
      )

      # Create a target for each library header that runs cppcheck on that file
      set(CPPHECK_TARGETS "")
      foreach(LIB_FILE ${CONFIGURE_DANALYSIS_FILES})
        string(REGEX REPLACE "/" "-" TARGET_NAME ${LIB_FILE})
        set(CPPCHECK_TARGET_NAME "cppcheck-${TARGET_NAME}")
        add_custom_target(
          ${CPPCHECK_TARGET_NAME}
          COMMAND ${CPPCHECK_EXE} ${CPPCHECK_ARGS}
          ${LIB_FILE}
        )
      set(CPPCHECK_TARGETS ${CPPCHECK_TARGETS} "${CPPCHECK_TARGET_NAME}")
      endforeach()

      # Custom target to run cppcheck on all library files sequentially
      add_custom_target(cppcheck)
      add_dependencies(cppcheck ${CPPCHECK_TARGETS})

      # Custom target to run cppcheck on all library files at once
      add_custom_target(
        cppcheck-all
        COMMAND ${CPPCHECK_EXE} ${CPPCHECK_ARGS}
        ${CONFIGURE_DANALYSIS_FILES}
      )

      set(STATIC_ANALYSIS_TARGETS ${STATIC_ANALYSIS_TARGETS} cppcheck-all)

    endif()
  else()
    message(STATUS "cppcheck:                     Disabled (enable with -DCPPCHECK=On)")
  endif()

  # --------------------------- clang-tidy ----------------------------

  if(CLANG_TIDY)
    # Find clang-tidy installation
    find_program(
      CLANG_TIDY_EXE
      NAMES "clang-tidy"
      DOC "Path to clang-tidy executable"
    )

    if(NOT CLANG_TIDY_EXE)
      message(FATAL_ERROR "clang-tidy enabled but could not find clang-tidy executable")
    else()
      message(STATUS "clang-tidy:                   Enabled (${CLANG_TIDY_EXE})")

      # Args for clang-tidy.
      set(CLANG_TIDY_ARGS
        "-quiet"
      )
      set(CLANG_TIDY_COMPILER_ARGS
        "-std=c++17"
        "-I${CONFIGURE_DANALYSIS_INCLUDES}"
        "-stdlib=libc++"
      )

      # Create a target for each library header that runs clang-tidy on that file
      set(CLANG_TIDY_TARGETS "")
      foreach(LIB_FILE ${CONFIGURE_DANALYSIS_FILES})
        string(REGEX REPLACE "/" "-" TARGET_NAME ${LIB_FILE})
        set(CLANG_TIDY_TARGET_NAME "clang-tidy-${TARGET_NAME}")
        add_custom_target(
          ${CLANG_TIDY_TARGET_NAME}
          COMMAND ${CLANG_TIDY_EXE} ${CLANG_TIDY_ARGS}
          ${LIB_FILE}
          --
          ${CLANG_TIDY_COMPILER_ARGS}
        )
        set(CLANG_TIDY_TARGETS ${CLANG_TIDY_TARGETS} "${CLANG_TIDY_TARGET_NAME}")
      endforeach()

      # Custom target to run clang-tidy on all library files sequentially
      add_custom_target(clang-tidy)
      add_dependencies(clang-tidy ${CLANG_TIDY_TARGETS})

      # Custom target to run clang-tidy on all library files at once
      add_custom_target(
        clang-tidy-all
        COMMAND ${CLANG_TIDY_EXE} ${CLANG_TIDY_ARGS}
        ${CONFIGURE_DANALYSIS_FILES}
        --
        ${CLANG_TIDY_COMPILER_ARGS}
      )

      set(STATIC_ANALYSIS_TARGETS ${STATIC_ANALYSIS_TARGETS} clang-tidy-all)

    endif()
  else()
    message(STATUS "clang-tidy:                   Disabled (enable with -DCLANG_TIDY=On)")
  endif()

  # ---------------------------- infer --------------------------------

  if(INFER)

    # Find infer executable
    find_program(
      INFER_EXE
      NAMES "infer"
      DOC "Path to infer executable"
    )

    # Find clang executable (required to run infer)
    find_program(
      CLANGXX_EXE
      NAMES "clang++"
      DOC "Path to clang++ executable"
    )

    if(NOT INFER_EXE)
      message(FATAL_ERROR "infer enabled but could not find infer executable")
    elseif(NOT CLANGXX_EXE)
      message(FATAL_ERROR "infer enabled but could not find clang++ executable, which is required by infer")
    else()
      message(STATUS "infer:                        Enabled (${INFER_EXE})")

      # Args for infer
      set(INFER_ARGS
        "--no-progress-bar"
        "--fail-on-issue"
      )
      set(INFER_COMPILER_ARGS
        "--std=c++17"
        "-I${CONFIGURE_DANALYSIS_INCLUDES}"
        "-stdlib=libc++"
      )

      # Create a target for each library header that runs infer on that file
      set(INFER_TARGETS "")
      foreach(LIB_FILE ${CONFIGURE_DANALYSIS_FILES})
        string(REGEX REPLACE "/" "-" TARGET_NAME ${LIB_FILE})
        set(INFER_TARGET_NAME "infer-${TARGET_NAME}")
        add_custom_target(
          ${INFER_TARGET_NAME}
          COMMAND ${INFER_EXE} run ${INFER_ARGS}
          --
          ${CLANGXX_EXE} ${INFER_COMPILER_ARGS}
          ${LIB_FILE}
        )
        set(INFER_TARGETS ${INFER_TARGETS} "${INFER_TARGET_NAME}")
      endforeach()

      # Custom target to run infer on all library files sequentially
      add_custom_target(infer)
      add_dependencies(infer ${INFER_TARGETS})
      
      # Custom target to run infer on all library files at once
      add_custom_target(
        infer-all
        COMMAND ${INFER_EXE} run ${INFER_ARGS}
        --
        ${CLANGXX_EXE} ${INFER_COMPILER_ARGS}
        ${CONFIGURE_DANALYSIS_FILES}
      )

      set(STATIC_ANALYSIS_TARGETS ${STATIC_ANALYSIS_TARGETS} infer-all)

    endif()
  else()
    message(STATUS "infer:                        Disabled (enable with -DINFER=On)")
  endif()

  # ----------------------- include-what-you-use --------------------------

  if(IWYU)
    # Find include-what-you-use executable
    find_program(
      IWYU_EXE
      NAMES "include-what-you-use" "iwyu"
      DOC "Path to include-what-you-use executable"
    )
    
    if(NOT IWYU_EXE)
      message(FATAL_ERROR "include-what-you-use enabled but could not find include-what-you-use executable")
    else()
      message(STATUS "include-what-you-use:         Enabled (${IWYU_EXE})")
     
      # Args for IWYU
      set(IWYU_ARGS 
        "-I${CONFIGURE_DANALYSIS_INCLUDES}"
        "-I${CONFIGURE_DANALYSIS_INCLUDES}/parlay"
        "-w"
        "-std=c++17"
      )

      # Create a target for each library header that runs infer on that file
      # Notice the strange exit code? We return $? - 2 since IWYU returns
      # 2 + number of errors. Makes sense, right?
      set(IWYU_TARGETS "")
      foreach(LIB_FILE ${CONFIGURE_DANALYSIS_FILES})
        string(REGEX REPLACE "/" "-" TARGET_NAME ${LIB_FILE})
        set(IWYU_TARGET_NAME "iwyu-${TARGET_NAME}")
        add_custom_target(
          ${IWYU_TARGET_NAME}
          COMMAND ${IWYU_EXE} ${IWYU_ARGS}
          ${LIB_FILE}
          || exit `expr $$? - 2`
        )
        set(IWYU_TARGETS ${IWYU_TARGETS} "${IWYU_TARGET_NAME}")
      endforeach()

      # Custom target to run include-what-you-use on all library files sequentially
      add_custom_target(iwyu)
      add_dependencies(iwyu ${IWYU_TARGETS})
      
      # include-what-you-use doesn't support compiling multiple files at once
      add_custom_target(iwyu-all)
      add_dependencies(iwyu-all iwyu)

      set(STATIC_ANALYSIS_TARGETS ${STATIC_ANALYSIS_TARGETS} iwyu-all)

    endif()
  else()
    message(STATUS "include-what-you-use:         Disabled (enable with -DIWYU=On)")
  endif()

  # ---------------------------- all linters ------------------------------

  # Target to run all enabled linters
  if(CPPCHECK OR CLANG_TIDY OR INFER OR IWYU)
    add_custom_target(analysis-all)
    add_dependencies(analysis-all ${STATIC_ANALYSIS_TARGETS})
  endif()
endmacro()