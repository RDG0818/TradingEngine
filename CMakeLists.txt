cmake_minimum_required(VERSION 3.12)

project(TradingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

find_package(pybind11 REQUIRED)

include_directories(
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SRC_DIR}/core
)

file(GLOB_RECURSE CORE_SOURCES ${PROJECT_SRC_DIR}/core/*.cpp)
file(GLOB_RECURSE BINDINGS_SOURCES ${PROJECT_SRC_DIR}/bindings/*.cpp)

pybind11_add_module(trading_core
    ${CORE_SOURCES}
    ${BINDINGS_SOURCES}
)

set_target_properties(trading_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/python/trading_engine"
)

# Google Test
add_subdirectory(googletest)

file(GLOB_RECURSE CPP_TEST_SOURCES "tests/cpp/*.cpp")

add_executable(my_tests 
    ${CPP_TEST_SOURCES}
    ${CORE_SOURCES}
)

target_link_libraries(my_tests GTest::gtest_main)
target_include_directories(my_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})