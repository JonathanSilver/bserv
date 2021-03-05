cmake_minimum_required(VERSION 3.10)

project(bserv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(bserv bserv.cpp)
target_link_libraries(bserv
                      pthread
                      boost_thread
                      boost_log
                      boost_log_setup
                      pqxx
                      pq
                      cryptopp)
