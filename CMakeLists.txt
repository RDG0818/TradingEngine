cmake_minimum_required(VERSION 3.12)

project(TradingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(pybind11 REQUIRED)


pybind11_add_module(trading_core
    bindings.cpp
    orderBook.cpp
    limitOrder.cpp
    marketOrder.cpp
    matchingEngine.cpp
)

set_target_properties(trading_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

# Google Test
add_subdirectory(googletest)
add_executable(my_tests 
    tests/orderBookTest.cpp 
    tests/eventDispatcherTest.cpp 
    tests/matchingEngineTest.cpp 
    orderBook.cpp 
    limitOrder.cpp 
    marketOrder.cpp 
    matchingEngine.cpp
)
target_link_libraries(my_tests GTest::gtest_main)
target_include_directories(my_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})