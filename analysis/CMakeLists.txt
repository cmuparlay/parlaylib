# List all files that should be considered for static analysis
include(danalysis)

configure_danalysis(
  INCLUDES
    ${PARLAY_INCLUDE_DIR}
  FILES
    ${PARLAY_INCLUDE_DIR}/parlay/scheduler.hpp
)
