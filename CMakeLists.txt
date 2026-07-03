project(VulConsole)

cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    PATHS /usr/lib/llvm-14/include
)
find_library(LIBCLANG_LIBRARY
    NAMES clang-14 libclang-14 clang
    PATHS /usr/lib/llvm-14/lib /lib/x86_64-linux-gnu
)
if (LIBCLANG_INCLUDE_DIR)
    include_directories(${LIBCLANG_INCLUDE_DIR})
endif()

file (GLOB_RECURSE LIB_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM LIB_CXX_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rtlzz_bridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rtlgen_rtlzz.cpp"
)

add_library(vulcore OBJECT ${LIB_CXX_SRC})

add_subdirectory(rtlzz EXCLUDE_FROM_ALL)

# Tools

file(GLOB TOOL_SOURCES tools/*.cpp)

foreach(tool ${TOOL_SOURCES})
    get_filename_component(tool_name ${tool} NAME_WE)

    add_executable(${tool_name} ${tool}  $<TARGET_OBJECTS:vulcore>)
    if (tool_name STREQUAL "vulrtlgen")
        target_sources(${tool_name} PRIVATE
            src/rtlzz_bridge.cpp
            src/rtlgen_rtlzz.cpp
        )
        target_include_directories(${tool_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rtlzz/src)
        target_link_libraries(${tool_name} predicate-expand-objects)
    endif()
    if (LIBCLANG_LIBRARY)
        target_link_libraries(${tool_name} ${LIBCLANG_LIBRARY})
    endif()
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

add_custom_target(copy_install_dir ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/install"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/install"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:vulsimgen>"
            "${CMAKE_CURRENT_BINARY_DIR}/install/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:vulrtlgen>"
            "${CMAKE_CURRENT_BINARY_DIR}/install/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/vullib"
            "${CMAKE_CURRENT_BINARY_DIR}/install/vullib"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/example"
            "${CMAKE_CURRENT_BINARY_DIR}/install/example"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/userguide"
            "${CMAKE_CURRENT_BINARY_DIR}/install/userguide"
    DEPENDS vulsimgen vulrtlgen copy_project_dirs
    COMMENT "Copying build artifacts into install directory"
)
