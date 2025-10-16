
project(VulSim)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS "-std=c++17 -O3 -DNDEBUG")
else ()
        set(CMAKE_CXX_FLAGS "-std=c++17 -g -O2")
endif ()

include_directories( . )

file(GLOB ALL_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# copy ./config.ini to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(vulsim ${ALL_CXX_SRC})



