project(VulConsole)

cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 20)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

file (GLOB_RECURSE LIB_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file (GLOB_RECURSE TUI_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/tui/*.cpp")

add_library(vulcore OBJECT ${LIB_CXX_SRC})

# TUI Executable

add_executable(vulconsole ${TUI_CXX_SRC} $<TARGET_OBJECTS:vulcore>)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(vulconsole PRIVATE -O2 -DNDEBUG)
else ()
    target_compile_options(vulconsole PRIVATE -g -O2)
endif ()

# Tools

file(GLOB TOOL_SOURCES tools/*.cpp)

foreach(tool ${TOOL_SOURCES})
    get_filename_component(tool_name ${tool} NAME_WE)

    add_executable(${tool_name} ${tool}  $<TARGET_OBJECTS:vulcore>)
endforeach()
