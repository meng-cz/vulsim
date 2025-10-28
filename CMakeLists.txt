project(VulConsole)

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

include_directories( ./src )

file(GLOB CORE_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB TUI_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.cpp")

add_executable(vulconsole ${CORE_CXX_SRC} ${TUI_CXX_SRC})


file(GLOB_RECURSE GUI_CXX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.cpp")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(vuldesigner ${CORE_CXX_SRC} ${GUI_CXX_SRC})
target_link_libraries(vuldesigner PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

