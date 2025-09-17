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

add_executable(sqd_tests
    test/doctest_main.cpp
    test/test_postselection.cpp
    test/test_subsampling.cpp
    test/test_configuration_recovery.cpp
)
target_include_directories(sqd_tests PRIVATE deps/doctest/doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(sqd_tests)

add_executable(sqd_benchmarks
    benchmark/benchmark_main.cpp
    benchmark/benchmark_subsampling.cpp
    benchmark/benchmark_configuration_recovery.cpp
)
target_include_directories(sqd_benchmarks PRIVATE deps/nanobench/src/include)
target_link_libraries(sqd_benchmarks PRIVATE nanobench)
