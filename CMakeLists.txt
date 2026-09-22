cmake_minimum_required(VERSION 3.25)
project(Ente LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Core library
add_library(ente_core
    src/core/hash.cpp
    src/identity/genesis_service.cpp
    src/history/rec.cpp
    src/epistemic/interpretation.cpp
    src/judgment/fixture.cpp
    src/rcc/reassessment.cpp
    src/constitution/verifier.cpp
    src/realization/runner.cpp
)

target_include_directories(ente_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Enable testing
enable_testing()

add_executable(test_genesis tests/test_genesis.cpp)
target_link_libraries(test_genesis PRIVATE ente_core)
add_test(NAME test_genesis COMMAND test_genesis)

add_executable(test_epistemic tests/test_epistemic.cpp)
target_link_libraries(test_epistemic PRIVATE ente_core)
add_test(NAME test_epistemic COMMAND test_epistemic)

add_executable(test_rec tests/test_rec.cpp)
target_link_libraries(test_rec PRIVATE ente_core)
add_test(NAME test_rec COMMAND test_rec)

add_executable(test_rcc tests/test_rcc.cpp)
target_link_libraries(test_rcc PRIVATE ente_core)
add_test(NAME test_rcc COMMAND test_rcc)

add_executable(test_verifier tests/test_verifier.cpp)
target_link_libraries(test_verifier PRIVATE ente_core)
add_test(NAME test_verifier COMMAND test_verifier)

add_executable(test_experiment_001 tests/test_experiment_001.cpp)
target_link_libraries(test_experiment_001 PRIVATE ente_core)
add_test(NAME test_experiment_001 COMMAND test_experiment_001)
