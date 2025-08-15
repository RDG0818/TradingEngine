cmake_minimum_required(VERSION 3.12)

project(TradingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(trading_engine main.cpp orderBook.cpp)

# Google Test
add_subdirectory(googletest)
add_executable(my_tests tests/orderBookTest.cpp orderBook.cpp)
target_link_libraries(my_tests GTest::gtest_main)
target_include_directories(my_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})