# See the following link for reasonable minimum version suggestions
# https://cliutils.gitlab.io/modern-cmake/chapters/intro/dodonot.html#what-minimum-to-choose-os-support
cmake_minimum_required(VERSION 3.20...4.1)

project(QiskitAddonSQD
    VERSION 0.0.0
)

# Use C++17 by default, or later if the user passes -DCMAKE_CXX_STANDARD
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "C++17 or later is required.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if (MSVC)
    add_compile_options(/W4)
else()
    # For GCC/Clang
    add_compile_options(-Wall -Wextra)
endif()

include_directories(include)

add_subdirectory(deps/doctest)
add_subdirectory(deps/nanobench)

# MPI and OpenMP
find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)

# BLAS
find_library(ACCELERATE_LIBRARY Accelerate)
if (ACCELERATE_LIBRARY)
    message(STATUS "Using Accelerate Framework")
else()
    set(BLA_VENDOR OpenBLAS)
    find_package(BLAS REQUIRED)
    include_directories(${BLAS_INCLUDE_DIRS})
    link_libraries(${BLAS_LIBRARIES})
endif()

# boost::dynamic_bitset
add_library(boost_dynamic_bitset INTERFACE)
target_include_directories(boost_dynamic_bitset INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/assert/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/config/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/container_hash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/describe/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/dynamic_bitset/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/integer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/move/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/mp11/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/static_assert/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/throw_exception/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/type_traits/include
)

add_library(bitset2 INTERFACE)
target_include_directories(bitset2 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/bitset2
)

add_library(sbd INTERFACE)
target_include_directories(sbd SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/sbd/include
)

add_executable(sqd_tests
    test/doctest_main.cpp
    test/test_postselection.cpp
    test/test_subsampling.cpp
    test/test_configuration_recovery.cpp
    test/test_fermion.cpp
    test/test_sbd_eigensolver.cpp
)
target_include_directories(sqd_tests PRIVATE
    deps/doctest/doctest
    ${MPI_C_INCLUDE_DIRS}
)
target_link_libraries(sqd_tests
    PRIVATE
    boost_dynamic_bitset
    bitset2
    sbd
    MPI::MPI_C
    OpenMP::OpenMP_CXX
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(sqd_tests)

add_executable(sqd_benchmarks
    benchmark/benchmark_main.cpp
    benchmark/benchmark_subsampling.cpp
    benchmark/benchmark_configuration_recovery.cpp
)
target_include_directories(sqd_benchmarks PRIVATE deps/nanobench/src/include)
target_link_libraries(sqd_benchmarks
    PRIVATE
    nanobench
    boost_dynamic_bitset
    bitset2
    sbd
)
