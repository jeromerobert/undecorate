cmake_minimum_required(VERSION 3.0)
find_package(X11 REQUIRED)
add_executable(undecorate undecorate.c)
target_link_libraries(undecorate ${X11_X11_LIB})
