# List all files that should be considered for static analysis
include(danalysis)

configure_danalysis(
  INCLUDES
    ${PARLAY_INCLUDE_DIR}
    ${PARLAY_INCLUDE_DIR}/parlay
  FILES
    ${PARLAY_INCLUDE_DIR}/parlay/internal/work_stealing_job.h
    ${PARLAY_INCLUDE_DIR}/parlay/parallel.h
    ${PARLAY_INCLUDE_DIR}/parlay/range.h
    ${PARLAY_INCLUDE_DIR}/parlay/scheduler.h
    ${PARLAY_INCLUDE_DIR}/parlay/slice.h
)
