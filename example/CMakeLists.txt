
add_executable(Example main.cpp)

target_link_libraries(Example PRIVATE statdeps)

set_target_properties(Example PROPERTIES CXX_STANDARD 17)
