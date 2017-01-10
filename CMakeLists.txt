# Minimum cmake version required
cmake_minimum_required(VERSION 2.8.7 FATAL_ERROR)

# Project name
set(PROJECT Hermes)
project(${PROJECT} CXX)

#compilation operations
IF (WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /O2")
ELSE ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Werror -pthread -g")
ENDIF (WIN32)

include_directories(libs)
include_directories(include)
include_directories(libs/Catch/single_include)

add_executable(tests_hermes tests/main.cpp)
