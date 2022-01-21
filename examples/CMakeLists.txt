
set(EXAMPLES "primes")

function(add_example NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE parlay)
endfunction()

foreach(example ${EXAMPLES})
  add_example(${example})
endforeach()






