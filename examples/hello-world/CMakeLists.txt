cmake_minimum_required(VERSION 3.10)

project(hello)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../WebApp/bserv bserv)
add_executable(main main.cpp)
target_link_libraries(main PUBLIC bserv)
