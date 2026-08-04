cmake_minimum_required(VERSION 3.14)
project(TradingEngine VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

# Dependencies
find_package(Boost REQUIRED)

# GoogleTest
add_subdirectory(googletest EXCLUDE_FROM_ALL)

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(benchmark EXCLUDE_FROM_ALL)

# ftxui (TUI library)
include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ftxui)

# Core C++ library (no Python, no TUI)
# NOTE: new trader sources (informed_trader, noise_trader) and tui sources
# are added in later tasks. Only currently-existing sources are listed here.
set(CORE_SOURCES
    src/engine/event_bus.cpp
    src/engine/exchange.cpp
    src/engine/order_book.cpp
    src/engine/order_matcher.cpp
    src/engine/portfolio.cpp
    src/market/trader.cpp
    src/market/trader_registry.cpp
    src/market/traders/market_maker.cpp
    src/market/traders/informed_trader.cpp
    src/market/traders/noise_trader.cpp
    src/tui/order_command_parser.cpp
)

add_library(core_lib STATIC ${CORE_SOURCES})
target_include_directories(core_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/moodycamel
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(core_lib PUBLIC Boost::boost)
set_target_properties(core_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# TUI binary
add_executable(trading_engine
    src/main.cpp
    src/tui/tui.cpp
)
target_include_directories(trading_engine PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(trading_engine PRIVATE
    core_lib
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# Tests
file(GLOB TEST_SOURCES "tests/cpp/*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "performanceTest.cpp")
add_executable(tests ${TEST_SOURCES})
target_include_directories(tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(tests PRIVATE
    GTest::gtest_main
    Boost::boost
    core_lib
)
include(GoogleTest)
gtest_discover_tests(tests)

# Benchmarks (not built by default)
add_executable(benchmarks EXCLUDE_FROM_ALL tests/cpp/performanceTest.cpp)
target_include_directories(benchmarks PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(benchmarks PRIVATE
    benchmark::benchmark
    Boost::boost
    core_lib
)
