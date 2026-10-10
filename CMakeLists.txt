cmake_minimum_required(VERSION 3.16)

project(warehouse_management_system
    VERSION 2.0.0
    DESCRIPTION "Role-based warehouse inventory management system in modern C++"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Core library: everything except the CLI entry point, so tests link the same code.
add_library(wms_core
    src/auth.cpp
    src/cli.cpp
    src/inventory.cpp
    src/money.cpp
    src/permissions.cpp
    src/pricing.cpp
    src/sha256.cpp
    src/stock.cpp
    src/text.cpp
    src/warehouse.cpp)
target_include_directories(wms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(wms_core PUBLIC /W4 /permissive-)
else()
    target_compile_options(wms_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_executable(warehouse src/main.cpp)
target_link_libraries(warehouse PRIVATE wms_core)

option(WMS_BUILD_TESTS "Build the unit test suite" ON)
if(WMS_BUILD_TESTS)
    enable_testing()
    add_executable(wms_tests
        tests/test_main.cpp
        tests/test_auth.cpp
        tests/test_cli.cpp
        tests/test_inventory.cpp
        tests/test_permissions.cpp
        tests/test_pricing.cpp
        tests/test_sha256.cpp
        tests/test_stock.cpp)
    target_link_libraries(wms_tests PRIVATE wms_core)
    target_compile_definitions(wms_tests PRIVATE WMS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME unit_tests COMMAND wms_tests)
endif()
