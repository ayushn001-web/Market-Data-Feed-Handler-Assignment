cmake_minimum_required(VERSION 3.16)
project(MarketDataFeedHandler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -fsanitize=address,undefined")

# --- Common library ---
add_library(common
    src/common/cache.cpp
    src/common/latency_tracker.cpp
    src/common/memory_pool.cpp
)
target_include_directories(common PUBLIC include src/common)

# --- Exchange Simulator (server) ---
add_executable(exchange_simulator
    src/server/main.cpp
    src/server/exchange_simulator.cpp
    src/server/tick_generator.cpp
    src/server/client_manager.cpp
)
target_include_directories(exchange_simulator PRIVATE include src/server src/common)
target_link_libraries(exchange_simulator common pthread)

# --- Feed Handler (client) ---
add_executable(feed_handler
    src/client/main.cpp
    src/client/feed_handler.cpp
    src/client/socket.cpp
    src/client/parser.cpp
    src/client/visualizer.cpp
)
target_include_directories(feed_handler PRIVATE include src/client src/common)
target_link_libraries(feed_handler common pthread)

# --- Tests ---
find_package(GTest QUIET)
if(GTest_FOUND)
    enable_testing()

    add_executable(unit_tests
        tests/test_parser.cpp
        tests/test_cache.cpp
        tests/test_latency_tracker.cpp
        tests/test_gbm.cpp
    )
    target_include_directories(unit_tests PRIVATE include src/common src/client src/server)
    target_link_libraries(unit_tests common GTest::GTest GTest::Main pthread)
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

# --- Benchmarks ---
add_executable(bench_latency benchmarks/bench_latency.cpp)
target_include_directories(bench_latency PRIVATE include src/common)
target_link_libraries(bench_latency common pthread)

add_executable(bench_parser benchmarks/bench_parser.cpp)
target_include_directories(bench_parser PRIVATE include src/common src/client)
target_link_libraries(bench_parser common pthread)