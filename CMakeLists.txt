cmake_minimum_required(VERSION 3.12)

project(TradingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

if (CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_options(-march=native)
endif()

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

include_directories(
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SRC_DIR}/core
    ${Boost_INCLUDE_DIRS}
)

file(GLOB_RECURSE CORE_SOURCES ${PROJECT_SRC_DIR}/core/*.cpp)

find_package(Boost REQUIRED)

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


file(GLOB_RECURSE CPP_TEST_SOURCES "tests/cpp/*.cpp")
list(REMOVE_ITEM CPP_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/performanceTest.cpp")

add_executable(tests 
    ${CPP_TEST_SOURCES}
    ${CORE_SOURCES}
)

target_link_libraries(tests GTest::gtest_main Boost::boost)
target_include_directories(tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Google Benchmark
FetchContent_Declare(
  benchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
FetchContent_MakeAvailable(benchmark)
enable_testing()

add_executable(benchmarks
    tests/cpp/performanceTest.cpp
    ${CORE_SOURCES}
)

target_link_libraries(benchmarks benchmark::benchmark Boost::boost)
target_include_directories(benchmarks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
