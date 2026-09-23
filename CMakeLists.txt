cmake_minimum_required(VERSION 3.25)
project(Ente LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and Sanitizers
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

option(ENABLE_SANITIZERS "Enable AddressSanitizer and LeakSanitizer" OFF)
if(ENABLE_SANITIZERS)
    message(STATUS "Enabling AddressSanitizer and LeakSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)

# Core library
add_library(ente_core
    src/core/hash.cpp
    src/core/ed25519.cpp
    src/core/prng.cpp
    src/identity/genesis_service.cpp
    src/identity/material_anchor.cpp
    src/authority/authority.cpp
    src/history/rec.cpp
    src/history/payloads.cpp
    src/epistemic/interpretation.cpp
    src/judgment/fixture.cpp
    src/rcc/reassessment.cpp
    src/assurance/runtime_assurance.cpp
    src/attestation/rats.cpp
    src/constitution/verifier.cpp
    src/realization/runner.cpp
)

target_include_directories(ente_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(ente_core PUBLIC cxx_std_23)
target_link_libraries(ente_core PUBLIC OpenSSL::Crypto)

# Enable testing
enable_testing()

add_executable(test_genesis tests/test_genesis.cpp)
target_link_libraries(test_genesis PRIVATE ente_core)
add_test(NAME test_genesis COMMAND test_genesis)

add_executable(test_material_binding tests/test_material_binding.cpp)
target_link_libraries(test_material_binding PRIVATE ente_core)
add_test(NAME test_material_binding COMMAND test_material_binding)

add_executable(test_prng tests/test_prng.cpp)
target_link_libraries(test_prng PRIVATE ente_core)
add_test(NAME test_prng COMMAND test_prng)

add_executable(test_authority tests/test_authority.cpp)
target_link_libraries(test_authority PRIVATE ente_core)
add_test(NAME test_authority COMMAND test_authority)

add_executable(test_epistemic tests/test_epistemic.cpp)
target_link_libraries(test_epistemic PRIVATE ente_core)
add_test(NAME test_epistemic COMMAND test_epistemic)

add_executable(test_rec tests/test_rec.cpp)
target_link_libraries(test_rec PRIVATE ente_core)
add_test(NAME test_rec COMMAND test_rec)

add_executable(test_authenticated_rec tests/test_authenticated_rec.cpp)
target_link_libraries(test_authenticated_rec PRIVATE ente_core)
add_test(NAME test_authenticated_rec COMMAND test_authenticated_rec)

add_executable(test_persistence_restart tests/test_persistence_restart.cpp)
target_link_libraries(test_persistence_restart PRIVATE ente_core)
add_test(NAME test_persistence_restart COMMAND test_persistence_restart)

add_executable(test_persistence_faults tests/test_persistence_faults.cpp)
target_link_libraries(test_persistence_faults PRIVATE ente_core)
add_test(NAME test_persistence_faults COMMAND test_persistence_faults)

add_executable(test_process_crash_recovery tests/test_process_crash_recovery.cpp)
target_link_libraries(test_process_crash_recovery PRIVATE ente_core)
add_test(NAME test_process_crash_recovery COMMAND test_process_crash_recovery)

add_executable(test_action_transaction tests/test_action_transaction.cpp)
target_link_libraries(test_action_transaction PRIVATE ente_core)
add_test(NAME test_action_transaction COMMAND test_action_transaction)

add_executable(test_rcc tests/test_rcc.cpp)
target_link_libraries(test_rcc PRIVATE ente_core)
add_test(NAME test_rcc COMMAND test_rcc)

add_executable(test_verifier tests/test_verifier.cpp)
target_link_libraries(test_verifier PRIVATE ente_core)
add_test(NAME test_verifier COMMAND test_verifier)

add_executable(test_experiment_001 tests/test_experiment_001.cpp)
target_link_libraries(test_experiment_001 PRIVATE ente_core)
add_test(NAME test_experiment_001 COMMAND test_experiment_001)

add_executable(test_vertical_lifecycle tests/test_vertical_lifecycle.cpp)
target_link_libraries(test_vertical_lifecycle PRIVATE ente_core)
add_test(NAME test_vertical_lifecycle COMMAND test_vertical_lifecycle)

# Chaos & Adversarial Invariant Attacks
add_executable(test_chaos_invariants tests/test_chaos_invariants.cpp)
target_link_libraries(test_chaos_invariants PRIVATE ente_core)
add_test(NAME test_chaos_invariants COMMAND test_chaos_invariants)

# Monte Carlo Stochastic Fuzzing & Statistical Pareto Benchmark (5,000 runs)
add_executable(test_monte_carlo_benchmark tests/test_monte_carlo_benchmark.cpp)
target_link_libraries(test_monte_carlo_benchmark PRIVATE ente_core)
add_test(NAME test_monte_carlo_benchmark COMMAND test_monte_carlo_benchmark)

# REC Longevity & Scalability Stress Test (10,000 continuous stream)
add_executable(test_rec_longevity tests/test_rec_longevity.cpp)
target_link_libraries(test_rec_longevity PRIVATE ente_core)
add_test(NAME test_rec_longevity COMMAND test_rec_longevity)

# Case Lab 001: Autonomous Vehicle (KitKat Analog)
add_executable(case_lab examples/case-lab/main.cpp)
target_link_libraries(case_lab PRIVATE ente_core)
target_include_directories(case_lab PRIVATE examples/case-lab)
add_test(NAME test_case_lab COMMAND case_lab)

# Case Lab 002: Clinical Infusion Pump under Telemetry Anomaly
add_executable(clinical_lab examples/clinical-lab/main.cpp)
target_link_libraries(clinical_lab PRIVATE ente_core)
target_include_directories(clinical_lab PRIVATE examples/clinical-lab)
add_test(NAME test_clinical_lab COMMAND clinical_lab)

# Application / Case Lab 003: ENTE Field Station & Observatory (Precision Irrigation)
add_executable(ente_field_station examples/field-station/main.cpp)
target_link_libraries(ente_field_station PRIVATE ente_core)
target_include_directories(ente_field_station PRIVATE examples/field-station)
add_test(NAME test_field_station COMMAND ente_field_station)

# CTest Suite Categorization Labels
set_tests_properties(test_monte_carlo_benchmark PROPERTIES LABELS "benchmark;monte_carlo")
set_tests_properties(test_rec_longevity PROPERTIES LABELS "long;performance;longevity")
set_tests_properties(test_chaos_invariants PROPERTIES LABELS "chaos;security")
set_tests_properties(test_authenticated_rec PROPERTIES LABELS "authentication;security")
set_tests_properties(test_process_crash_recovery PROPERTIES LABELS "crash;recovery;posix")
set_tests_properties(test_case_lab PROPERTIES LABELS "case_lab;domain")
set_tests_properties(test_clinical_lab PROPERTIES LABELS "clinical_lab;domain")
set_tests_properties(test_field_station PROPERTIES LABELS "field_station;visual;domain")

# ==============================================================================
# Installation & CMake Packaging Export
# ==============================================================================
install(TARGETS ente_core
    EXPORT EnteTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT EnteTargets
    FILE EnteTargets.cmake
    NAMESPACE Ente::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ente
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EnteConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnteConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/EnteConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ente
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/EnteConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EnteConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Ente
)
