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
    if(WIN32)
        add_link_options(-static -static-libgcc -static-libstdc++)
    endif()
endif()

# Global include directory
include_directories(${PROJECT_SOURCE_DIR}/include)

# -------------------------------------------------------------
# Support Library
# -------------------------------------------------------------
add_library(ptxlab_support STATIC
    src/support/version.cpp
    src/support/source_manager.cpp
    src/support/diagnostic.cpp
)
target_include_directories(ptxlab_support PUBLIC ${PROJECT_SOURCE_DIR}/include)

# -------------------------------------------------------------
# KC Front-End Library (ptxlab_kc)
# -------------------------------------------------------------
add_library(ptxlab_kc STATIC
    src/kc/lexer.cpp
    src/kc/ast_printer.cpp
    src/kc/parser.cpp
    src/kc/sema.cpp
    src/kc/ast_interpreter.cpp
)
target_include_directories(ptxlab_kc PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ptxlab_kc PUBLIC ptxlab_support)

# -------------------------------------------------------------
# GATE Laboratory Library & Executable (gate-lab)
# -------------------------------------------------------------
add_library(ptxlab_gate STATIC
    tools/gate-lab/first_follow.cpp
    tools/gate-lab/ll1_table.cpp
)
target_include_directories(ptxlab_gate PUBLIC ${PROJECT_SOURCE_DIR}/tools/gate-lab)

add_executable(gate-lab
    tools/gate-lab/main.cpp
)
target_link_libraries(gate-lab PRIVATE ptxlab_gate)

# -------------------------------------------------------------
# KC IR Library (ptxlab_ir)
# -------------------------------------------------------------
add_library(ptxlab_ir STATIC
    src/ir/instruction.cpp
    src/ir/function.cpp
    src/ir/ir_builder.cpp
    src/ir/ir_printer.cpp
    src/ir/lower.cpp
    src/ir/ir_verifier.cpp
    src/ir/ir_interpreter.cpp
)
target_include_directories(ptxlab_ir PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ptxlab_ir PUBLIC ptxlab_kc ptxlab_support)

# -------------------------------------------------------------
# PTX Code Generation & Analysis Library (ptxlab_ptx)
# -------------------------------------------------------------
add_library(ptxlab_ptx STATIC
    src/ptx/ptx_emitter.cpp
    src/ptx/ptx_parser.cpp
    src/ptx/ptx_validator.cpp
    src/ptx/ptx_interpreter.cpp
)
target_include_directories(ptxlab_ptx PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ptxlab_ptx PUBLIC ptxlab_ir ptxlab_kc ptxlab_support)

# -------------------------------------------------------------
# GPU Runner Library (ptxlab_gpu) — dynamic CUDA Driver API
# -------------------------------------------------------------
add_library(ptxlab_gpu STATIC
    src/gpu/gpu_runner.cpp
)
target_include_directories(ptxlab_gpu PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ptxlab_gpu PUBLIC ptxlab_ir ptxlab_kc ptxlab_support)

# -------------------------------------------------------------
# Compiler Executable (kcc)
# -------------------------------------------------------------
add_executable(kcc
    src/tools/kcc.cpp
)
target_link_libraries(kcc PRIVATE ptxlab_gpu ptxlab_ptx ptxlab_ir ptxlab_kc ptxlab_support)

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
    tests/unit/test_source_manager.cpp
    tests/unit/test_diagnostic.cpp
    tests/unit/test_lexer.cpp
    tests/unit/test_ast.cpp
    tests/unit/test_parser.cpp
    tests/unit/test_gate_lab.cpp
    tests/unit/test_sema.cpp
    tests/unit/test_ast_interpreter.cpp
    tests/unit/test_ir.cpp
    tests/unit/test_lower.cpp
    tests/unit/test_ir_verifier.cpp
    tests/unit/test_ir_interpreter.cpp
    tests/unit/test_ptx_emitter.cpp
    tests/unit/test_ptx_interpreter.cpp
    tests/unit/test_gpu_runner.cpp
)
target_link_libraries(unit_tests PRIVATE ptxlab_gpu ptxlab_ptx ptxlab_ir ptxlab_kc ptxlab_support ptxlab_gate GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
