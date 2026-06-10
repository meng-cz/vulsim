project(VulConsole)

cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 20)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

file (GLOB_RECURSE LIB_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(vulcore OBJECT ${LIB_CXX_SRC})

# Tools

file(GLOB TOOL_SOURCES tools/*.cpp)

foreach(tool ${TOOL_SOURCES})
    get_filename_component(tool_name ${tool} NAME_WE)

    add_executable(${tool_name} ${tool}  $<TARGET_OBJECTS:vulcore>)
endforeach()

# Copy project runtime directories to the build directory on each build:
# - vullib: only top-level files (non-recursive)
# - example: full directory recursively
file(GLOB VULLIB_TOP_LEVEL_ITEMS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "vullib/*")
set(VULLIB_COPY_COMMANDS "")
foreach(item ${VULLIB_TOP_LEVEL_ITEMS})
    if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
        list(APPEND VULLIB_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${item}"
            "${CMAKE_CURRENT_BINARY_DIR}/vullib/"
        )
    endif()
endforeach()

add_custom_target(copy_project_dirs ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/vullib"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/example"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/userguide"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/vullib"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/example"
            "${CMAKE_CURRENT_BINARY_DIR}/example"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/doc/userguide"
            "${CMAKE_CURRENT_BINARY_DIR}/userguide"
    ${VULLIB_COPY_COMMANDS}
    COMMENT "Copying vullib, example, and userguide into build directory"
)
