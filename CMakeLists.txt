project(VulConsole)

cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

set(REQUIRED_LLVM_MAJOR_VERSION 18)

find_program(LLVM_CONFIG_EXECUTABLE
    NAMES llvm-config-${REQUIRED_LLVM_MAJOR_VERSION}
    PATHS
        "$ENV{LLVM_ROOT}/bin"
        "${LLVM_ROOT}/bin"
        "/usr/lib/llvm-${REQUIRED_LLVM_MAJOR_VERSION}/bin"
    NO_DEFAULT_PATH
)
find_program(LLVM_CONFIG_EXECUTABLE
    NAMES llvm-config-${REQUIRED_LLVM_MAJOR_VERSION}
)

set(_LLVM18_ROOT_HINTS
    "$ENV{LLVM_ROOT}"
    "${LLVM_ROOT}"
    "/usr/lib/llvm-${REQUIRED_LLVM_MAJOR_VERSION}"
)

set(_LLVM18_INCLUDE_HINTS "")
set(_LLVM18_LIBRARY_HINTS "")
if (LLVM_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
        OUTPUT_VARIABLE _LLVM18_CONFIG_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
        OUTPUT_VARIABLE _LLVM18_CONFIG_LIBRARY_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${LLVM_CONFIG_EXECUTABLE} --version
        OUTPUT_VARIABLE _LLVM18_CONFIG_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT _LLVM18_CONFIG_VERSION MATCHES "^${REQUIRED_LLVM_MAJOR_VERSION}\\.")
        message(FATAL_ERROR
            "llvm-config executable '${LLVM_CONFIG_EXECUTABLE}' reports version "
            "'${_LLVM18_CONFIG_VERSION}', but LLVM ${REQUIRED_LLVM_MAJOR_VERSION}.x is required."
        )
    endif()
    list(APPEND _LLVM18_INCLUDE_HINTS "${_LLVM18_CONFIG_INCLUDE_DIR}")
    list(APPEND _LLVM18_LIBRARY_HINTS "${_LLVM18_CONFIG_LIBRARY_DIR}")
endif()

find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    HINTS ${_LLVM18_INCLUDE_HINTS}
    PATHS ${_LLVM18_ROOT_HINTS}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
)
find_library(LIBCLANG_LIBRARY
    NAMES clang-${REQUIRED_LLVM_MAJOR_VERSION} libclang-${REQUIRED_LLVM_MAJOR_VERSION} clang libclang
    HINTS ${_LLVM18_LIBRARY_HINTS}
    PATHS
        ${_LLVM18_ROOT_HINTS}
        /usr/lib/x86_64-linux-gnu
        /lib/x86_64-linux-gnu
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH
)
if (NOT LIBCLANG_INCLUDE_DIR OR NOT LIBCLANG_LIBRARY)
    message(FATAL_ERROR
        "LLVM/Clang ${REQUIRED_LLVM_MAJOR_VERSION} development files were not found. "
        "Install llvm-${REQUIRED_LLVM_MAJOR_VERSION}-dev, clang-${REQUIRED_LLVM_MAJOR_VERSION}, "
        "and libclang-${REQUIRED_LLVM_MAJOR_VERSION}-dev, or pass LLVM_ROOT=/path/to/llvm-${REQUIRED_LLVM_MAJOR_VERSION} "
        "or -DLIBCLANG_INCLUDE_DIR=/path/to/include -DLIBCLANG_LIBRARY=/path/to/libclang."
    )
endif()
set(_CLANG_VERSION_INC "${LIBCLANG_INCLUDE_DIR}/clang/Basic/Version.inc")
if (NOT EXISTS "${_CLANG_VERSION_INC}")
    message(FATAL_ERROR
        "Clang version file was not found at '${_CLANG_VERSION_INC}'. "
        "LLVM/Clang ${REQUIRED_LLVM_MAJOR_VERSION} development headers are required."
    )
endif()
file(READ "${_CLANG_VERSION_INC}" _CLANG_VERSION_INC_CONTENT)
if (NOT _CLANG_VERSION_INC_CONTENT MATCHES "#define CLANG_VERSION_MAJOR ${REQUIRED_LLVM_MAJOR_VERSION}")
    message(FATAL_ERROR
        "The selected Clang headers in '${LIBCLANG_INCLUDE_DIR}' are not LLVM/Clang "
        "${REQUIRED_LLVM_MAJOR_VERSION}. Please clear the CMake cache or pass "
        "LLVM_ROOT=/path/to/llvm-${REQUIRED_LLVM_MAJOR_VERSION}."
    )
endif()
message(STATUS "Using LLVM/Clang ${REQUIRED_LLVM_MAJOR_VERSION} libclang include dir: ${LIBCLANG_INCLUDE_DIR}")
message(STATUS "Using LLVM/Clang ${REQUIRED_LLVM_MAJOR_VERSION} libclang library: ${LIBCLANG_LIBRARY}")
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
