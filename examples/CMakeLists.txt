cmake_minimum_required(VERSION 3.10)

project(bserv_examples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../bserv bserv)

add_executable(hello hello.cpp)
target_link_libraries(hello PUBLIC bserv)

add_executable(routing routing.cpp)
target_link_libraries(routing PUBLIC bserv)
