project(VulConsole)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug")
endif()

include_directories( ./src )

file(GLOB CORE_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB TUI_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.cpp")

file(GLOB_RECURSE GUI_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.cpp")


# Create an object library for core sources to avoid recompilation

add_library(core_objects OBJECT ${CORE_CXX_SRC})

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(core_objects PRIVATE -O3 -DNDEBUG)
else ()
    target_compile_options(core_objects PRIVATE -g -O2)
endif ()


# TUI Executable

add_executable(vulconsole ${TUI_CXX_SRC} $<TARGET_OBJECTS:core_objects>)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(vulconsole PRIVATE -O3 -DNDEBUG)
else ()
    target_compile_options(vulconsole PRIVATE -g -O2)
endif ()


# GUI Executable

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(vuldesigner ${GUI_CXX_SRC} $<TARGET_OBJECTS:core_objects>)
target_link_libraries(vuldesigner PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(vuldesigner PRIVATE -O3 -DNDEBUG)
else ()
    target_compile_options(vuldesigner PRIVATE -g -O2)
endif ()


