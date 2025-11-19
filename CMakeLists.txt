cmake_minimum_required(VERSION 3.12)

project(TradingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(PROJECT_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

include_directories(
    ${PROJECT_INCLUDE_DIR}
    ${PROJECT_SRC_DIR}/core
)

file(GLOB_RECURSE CORE_SOURCES ${PROJECT_SRC_DIR}/core/*.cpp)

# Google Test
add_subdirectory(googletest)

file(GLOB_RECURSE CPP_TEST_SOURCES "tests/cpp/*.cpp")

add_executable(my_tests 
    ${CPP_TEST_SOURCES}
    ${CORE_SOURCES}
)

target_link_libraries(my_tests GTest::gtest_main)
target_include_directories(my_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})