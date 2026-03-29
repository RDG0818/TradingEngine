cmake_minimum_required(VERSION 3.12)
project(TradingEngine VERSION 2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_options(-march=native)
endif()

find_package(Boost REQUIRED)

set(CORE_SOURCES
    src/event_bus.cpp
    src/order_book.cpp
    src/order_matcher.cpp
    src/portfolio.cpp
    src/trader_registry.cpp
    src/market_events.cpp
    src/exchange.cpp
    src/trader.cpp
    src/traders/market_maker.cpp
    src/traders/momentum.cpp
    src/traders/mean_reversion.cpp
    src/traders/twap.cpp
    src/traders/trend_follower.cpp
    src/traders/random_limit.cpp
    src/traders/random_market.cpp
    src/traders/panic.cpp
)

add_library(core_lib STATIC ${CORE_SOURCES})
set_target_properties(core_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(core_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/moodycamel
    ${Boost_INCLUDE_DIRS}
)

add_subdirectory(googletest)
add_subdirectory(benchmark)
enable_testing()

file(GLOB CPP_TEST_SOURCES "tests/cpp/*.cpp")
list(REMOVE_ITEM CPP_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/performanceTest.cpp")

add_executable(tests ${CPP_TEST_SOURCES})
target_link_libraries(tests GTest::gtest_main Boost::boost core_lib)
target_include_directories(tests PUBLIC ${PROJECT_SOURCE_DIR})

add_executable(benchmarks EXCLUDE_FROM_ALL tests/cpp/performanceTest.cpp)
target_link_libraries(benchmarks benchmark::benchmark Boost::boost core_lib)
target_include_directories(benchmarks PUBLIC ${PROJECT_SOURCE_DIR})

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
add_subdirectory(pybind11)
pybind11_add_module(trading_engine_py src/python_bindings.cpp)
target_link_libraries(trading_engine_py PRIVATE core_lib Boost::boost)
set_target_properties(trading_engine_py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/backend
)
