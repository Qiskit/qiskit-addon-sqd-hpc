# See the following link for reasonable minimum version suggestions
# https://cliutils.gitlab.io/modern-cmake/chapters/intro/dodonot.html#what-minimum-to-choose-os-support
cmake_minimum_required(VERSION 3.20)

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

include_directories(include)

add_subdirectory(deps/doctest)
add_subdirectory(deps/nanobench)

function(find_package_fallback package_name subdir)
    find_package(${package_name} QUIET)
    if(${package_name}_FOUND)
        message(STATUS "${package_name} found")
    else()
        message(STATUS "${package_name} not found, using version in ${subdir}")
        set("${package_name}_VENDORED" TRUE PARENT_SCOPE)
        add_subdirectory(${subdir})
    endif()
endfunction()

# FIXME: These are required only by the sqd-hdf5-chemistry executable;
# ideally we could make them optional overall
find_package_fallback(HighFive deps/highfive)
find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)
set(xtensor_INCLUDE_DIRS
    deps/xtensor/include
    deps/xtl/include
)

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

# sbd library
find_package(BLAS REQUIRED)
add_library(sbd INTERFACE)
target_include_directories(sbd INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/sbd/include
    ${MPI_CXX_INCLUDE_DIRS}
    ${BLAS_INCLUDE_DIRS}
)
target_link_libraries(sbd
    INTERFACE
    ${BLAS_LIBRARIES}
    OpenMP::OpenMP_CXX
    MPI::MPI_CXX
)

add_definitions(-DH5_USE_XTENSOR)

add_executable(sqd_tests
    test/doctest_main.cpp
    test/test_postselection.cpp
    test/test_subsampling.cpp
    test/test_configuration_recovery.cpp
)
target_include_directories(sqd_tests PRIVATE deps/doctest/doctest)
target_link_libraries(sqd_tests
    PRIVATE
    boost_dynamic_bitset
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(sqd_tests)

add_executable(sqd_benchmarks
    benchmark/benchmark_main.cpp
    benchmark/benchmark_subsampling.cpp
    benchmark/benchmark_configuration_recovery.cpp
)
target_include_directories(sqd_benchmarks PRIVATE deps/nanobench/src/include)
target_link_libraries(sqd_benchmarks PRIVATE nanobench)

add_executable(sqd-hdf5-chemistry src/sqd-hdf5-chemistry.cpp)
target_include_directories(sqd-hdf5-chemistry
    PRIVATE
    ${xtensor_INCLUDE_DIRS}
)
target_link_libraries(sqd-hdf5-chemistry
    PRIVATE
    HighFive
    boost_dynamic_bitset
    sbd
    MPI::MPI_CXX
)
