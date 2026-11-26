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
    src/lru_policy.cpp
    src/lfu_policy.cpp
    src/text.cpp
    src/policy_factory.cpp
    src/pubsub.cpp
    src/connection_state.cpp
    src/connection.cpp
    src/socket.cpp
    src/thread_pool.cpp
    src/server.cpp
    src/commands.cpp
    src/command_factory.cpp
)
find_package(Threads REQUIRED)
target_include_directories(miniredis_core PUBLIC include)
target_link_libraries(miniredis_core PUBLIC Threads::Threads)
miniredis_set_options(miniredis_core)

# Command line parsing: system CLI11 if present, otherwise a pinned release.
find_package(CLI11 2.3 QUIET)
if(NOT CLI11_FOUND)
    include(FetchContent)
    FetchContent_Declare(CLI11
        URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.2.tar.gz
        URL_HASH SHA256=f2d893a65c3b1324c50d4e682c0cdc021dd0477ae2c048544f39eed6654b699a
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(CLI11)
endif()

add_executable(miniredis_server src/main.cpp)
target_link_libraries(miniredis_server PRIVATE miniredis_core CLI11::CLI11)
miniredis_set_options(miniredis_server)

if(MINIREDIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
