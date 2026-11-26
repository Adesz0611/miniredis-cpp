# Use the system GoogleTest if present, otherwise fetch a pinned release.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

add_executable(miniredis_tests
    test_store.cpp
    test_eviction.cpp
    test_thread_pool.cpp
    test_socket.cpp
    test_server.cpp
    test_server_pubsub.cpp
    test_pubsub.cpp
    test_text.cpp
    test_policy_factory.cpp
    test_commands.cpp
    test_command_factory.cpp
)
target_link_libraries(miniredis_tests PRIVATE miniredis_core GTest::gtest_main)
miniredis_set_options(miniredis_tests)
gtest_discover_tests(miniredis_tests)
