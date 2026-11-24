cmake_minimum_required(VERSION 3.21)

project(miniredis
    VERSION 0.1.0
    DESCRIPTION "Multithreaded in-memory key-value server (production software development project)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINIREDIS_BUILD_TESTS "Build unit tests" ON)
option(MINIREDIS_ENABLE_COVERAGE "Instrument for gcov/lcov coverage" OFF)
option(MINIREDIS_ENABLE_ASAN "AddressSanitizer + UBSan" OFF)
option(MINIREDIS_ENABLE_TSAN "ThreadSanitizer" OFF)
option(MINIREDIS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

include(cmake/CompilerOptions.cmake)

add_library(miniredis_core
    src/store.cpp
    src/commands.cpp
)
target_include_directories(miniredis_core PUBLIC include)
miniredis_set_options(miniredis_core)

add_executable(miniredis_server src/main.cpp)
target_link_libraries(miniredis_server PRIVATE miniredis_core)
miniredis_set_options(miniredis_server)

if(MINIREDIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
