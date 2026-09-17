# Developer guide

Development of the `qiskit-addon-sqd-hpc` package takes place [on GitHub](https://github.com/Qiskit/qiskit-addon-sqd-hpc).
The [Contributing to Qiskit](https://github.com/Qiskit/qiskit/blob/main/CONTRIBUTING.md) guide may serve as a
useful starting point, as this package is designed to be used with [Qiskit].

This package is written in modern C++, and all code must be compilable with C++17 or any later version of the standard.

The CI workflows are described at [`.github/workflows/README.md`](.github/workflows/README.md).

We use [Sphinx], [Doxygen], and [Breathe] for documentation and [reno] for release notes.

## Coverage

We prefer 100% coverage in all new code.

The coverage workflow reports line coverage to [Coveralls], and passes
`--exclude-throw-branches` to gcovr so that the reported percentage is not
dominated by branches no test can reach.  In a header-only template library
GCC emits a branch to the exception unwinder for anything that may throw, so
`v.emplace_back(x)` and `return {a, b};` each carry a permanently uncovered
branch despite containing no conditional; gcov labels these `(throw)`.  Around
40% of all branches in this project were of that kind.

To see which conditionals the tests never exercise both ways, build with
coverage instrumentation and run the reporting script:

```sh
cmake -S . -B build -DCMAKE_CXX_FLAGS="--coverage" -G Ninja
cmake --build build
./build/sqd_tests
gcovr --json coverage.json --root . \
    --exclude build --exclude deps --exclude benchmark --exclude-throw-branches
python3 tools/untested-conditionals.py coverage.json
```

The script lists lines under `include/` that ran but never took every outcome,
most often an input-validation guard that no test triggers.  The same output
appears in the job summary of every coverage run in CI.  It filters out
branches that cannot be covered or that do not correspond to a conditional in
this code base; `tools/untested-conditionals.py` documents each filter and why
it is needed.  Note that it reports what is *untested*, which is not the same
as what is wrong.

[Coveralls]: https://coveralls.io/github/Qiskit/qiskit-addon-sqd-hpc

[Qiskit]: https://www.ibm.com/quantum/qiskit
[Sphinx]: https://www.sphinx-doc.org/
[Doxygen]: https://www.doxygen.nl/
[Breathe]: https://breathe.readthedocs.io/en/latest/
[reno]: https://docs.openstack.org/reno/
