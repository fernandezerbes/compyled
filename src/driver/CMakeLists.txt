
add_executable(cpy
  cpy.cpp
)

target_link_libraries(cpy PRIVATE cpy_frontend cpy_ir)
