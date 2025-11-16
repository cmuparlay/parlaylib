set(HELPER "from_pbbs_to_sym")

function(add_helper NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE parlay)
endfunction()

foreach(helper ${HELPER})
  add_helper(${helper})
endforeach()
