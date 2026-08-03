cmake_minimum_required(VERSION 3.15)

project(binance_aggregator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(ixwebsocket REQUIRED)

enable_testing()

add_executable(binance_aggregator 
    binance_aggregator.cpp
    fixedpoint.cpp
    settings.cpp
    jsonparser.cpp
    aggregator.cpp
)

target_link_libraries(binance_aggregator PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ixwebsocket::ixwebsocket
    GTest::gtest
)

add_executable(unit_tests 
    fixedpoint.cpp
    settings.cpp
    jsonparser.cpp
    aggregator.cpp
    tests/test_fixedpoint.cpp
    tests/test_settings.cpp
    tests/test_jsonparser.cpp
    tests/test_aggregator.cpp
)

target_link_libraries(unit_tests PRIVATE 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
