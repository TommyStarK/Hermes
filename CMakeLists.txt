# Minimum cmake version required
cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)

# Project name
set(PROJECT Hermes)
project(${PROJECT} CXX)

#compilation operations
IF (WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /O2 -coverage")
ELSE ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -pedantic -pthread -coverage -g -o3")
ENDIF (WIN32)

include_directories(libs)
include_directories(include)
include_directories(libs/Catch/single_include)

IF (WIN32)
add_executable(tests_hermes tests/windows/main.cpp)
ELSE ()
add_executable(tests_hermes tests/unix/main.cpp)
ENDIF (WIN32)
