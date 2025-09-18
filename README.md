![Platform](https://img.shields.io/badge/%F0%9F%92%BB%20Platform-Linux%20%7C%20macOS%20%7C%20Windows-informational)
[![C++17](https://img.shields.io/badge/C++17-%2300599C.svg?logo=c%2B%2B&logoColor=white)](#)
[![Docs (dev)](https://img.shields.io/badge/%F0%9F%93%84%20Docs-stable-blue.svg)](https://qiskit.github.io/qiskit-addon-sqd-hpc/)
[![License](https://img.shields.io/github/license/Qiskit/qiskit-addon-sqd-hpc?label=License)](LICENSE.txt)
[![Tests](https://github.com/Qiskit/qiskit-addon-sqd-hpc/actions/workflows/test_latest_versions.yml/badge.svg)](https://github.com/Qiskit/qiskit-addon-sqd-hpc/actions/workflows/test_latest_versions.yml)
[![Coverage](https://coveralls.io/repos/github/Qiskit/qiskit-addon-sqd-hpc/badge.svg?branch=main)](https://coveralls.io/github/Qiskit/qiskit-addon-sqd-hpc?branch=main)

# qiskit-addon-sqd-hpc

This repository contains an HPC-ready implementation of the [Qiskit addon for Sample-based Quantum Diagonalization (SQD)](https://github.com/Qiskit/qiskit-addon-sqd).

## Features

- Modern C++17 template library, compatible with standard (STL) interfaces.
- Provides low-level functions for performing postselection, subsampling, and configuration recovery.
- Able to integrate with the [sbd eigensolver](https://github.com/r-ccs-cms/sbd), which was developed at RIKEN to enable large-scale SQD calculations.  See the [Qiskit 2.2 C API demo repository], in which a single binary harnessing both OpenMP and MPI-level parallelism is compiled for an HPC cluster.
- Cross-architecture, tested on x86_64 and ARMv8.  Cross-platform, tested on Linux, macOS, and Windows.
- Compile with exceptions enabled or disabled -- your choice.  [RTTI](https://en.wikipedia.org/wiki/Run-time_type_information) is optional, too.
- Complete API documentation.
- Fully tested, with a test suite based on [doctest](https://github.com/doctest/doctest).
- Performant, with a micro-benchmark suite based on [nanobench](https://github.com/martinus/nanobench).

## Documentation

All documentation is available at https://qiskit.github.io/qiskit-addon-sqd-hpc/.

## How to build

The library itself consists only of header files, so it can be used regardless of build system.  The tests and benchmarks are meant to be compiled with `cmake`.

### Clone dependencies for testing

To clone repositories needed for tests and benchmarks, run:

```sh
git submodule update --init --recursive
```

### Compile

```sh
mkdir -p build
cd build
cmake ..
cmake --build . -j4
```

If you wish to use a different build system, pass the `-G` option to the first invocation of `cmake`.  For instance, to use `ninja`, run `cmake .. -G Ninja`.

To compile with a version of C++ later than C++17, one must pass the `-DCMAKE_CXX_STANDARD` option to the first invocation of `cmake`.  For instance, to compile with the C++20 standard, run `cmake .. -DCMAKE_CXX_STANDARD=20`.

To compile with both RTTI and exceptions disabled, one must pass the appropriate flags to `cmake`.  For instance:

```sh
mkdir -p build
cd build
cmake .. -DCMAKE_CXX_FLAGS="-fno-rtti -fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS"
cmake --build . -t sqd_tests
```

The above example builds only the `sqd_tests` target, as the benchmarks cannot be compiled with exceptions disabled because [nanobench uses RTTI and exceptions](https://github.com/martinus/nanobench/pull/125) internally.

### Run test suite

To run the test suite, navigate to the `build` directory, then run

```sh
./sqd_tests
```

### Run benchmark suite

To run the benchmark suite, navigate to the `build` directory, then run

```sh
./sqd_benchmarks
```

## Deprecation policy

We follow [semantic versioning](https://semver.org/) and are guided by the principles in
[Qiskit's deprecation policy](https://github.com/Qiskit/qiskit/blob/main/DEPRECATION.md).
We may occasionally make breaking changes in order to improve the user experience.
When possible, we will keep old interfaces and mark them as deprecated, as long as they can co-exist with the
new ones.
Each substantial improvement, breaking change, or deprecation will be documented in the
[release notes](https://qiskit.github.io/qiskit-addon-sqd-hpc/release-notes.html).

## Contributing

The source code is available [on GitHub](https://github.com/Qiskit/qiskit-addon-sqd-hpc).

The developer guide is located at [CONTRIBUTING.md](https://github.com/Qiskit/qiskit-addon-sqd-hpc/blob/main/CONTRIBUTING.md)
in the root of this project's repository.
By participating, you are expected to uphold Qiskit's [code of conduct](https://github.com/Qiskit/qiskit/blob/main/CODE_OF_CONDUCT.md).

We use [GitHub issues](https://github.com/Qiskit/qiskit-addon-sqd-hpc/issues) for tracking requests and bugs.

## License

[Apache License 2.0](LICENSE.txt)
