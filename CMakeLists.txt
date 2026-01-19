project(VulConsole)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

file (GLOB_RECURSE ALL_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# TUI Executable

add_executable(vulconsole ${ALL_CXX_SRC})

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(vulconsole PRIVATE -O3 -DNDEBUG)
else ()
    target_compile_options(vulconsole PRIVATE -g -O2)
endif ()


