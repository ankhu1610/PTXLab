cmake_minimum_required(VERSION 3.24)
project(PTXLab VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Ensure portable FetchContent downloads work across diverse Windows environments
set(CMAKE_TLS_VERIFY OFF)

# Enforce C++20 strictly across all targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd / editor integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Strict warnings to maintain high compiler quality
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter)
endif()

# Global include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# -------------------------------------------------------------
# Support Library
# -------------------------------------------------------------
add_library(ptxlab_support STATIC
    src/support/version.cpp
)
target_include_directories(ptxlab_support PUBLIC ${PROJECT_SOURCE_DIR}/include)

# -------------------------------------------------------------
# Compiler Executable (kcc)
# -------------------------------------------------------------
add_executable(kcc
    src/tools/kcc.cpp
)
target_link_libraries(kcc PRIVATE ptxlab_support)

# -------------------------------------------------------------
# Testing: GoogleTest integration via FetchContent
# -------------------------------------------------------------
enable_testing()
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

# For Windows/MSVC: prevent overriding runtime settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    tests/unit/test_smoke.cpp
)
target_link_libraries(unit_tests PRIVATE ptxlab_support GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
