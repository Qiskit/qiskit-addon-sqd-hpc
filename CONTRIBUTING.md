# Developer guide

Development of the `qiskit-addon-sqd-hpc` package takes place [on GitHub](https://github.com/Qiskit/qiskit-addon-sqd-hpc).
The [Contributing to Qiskit](https://github.com/Qiskit/qiskit/blob/main/CONTRIBUTING.md) guide may serve as a
useful starting point, as this package is designed to be used with [Qiskit].

This package is written in modern C++, and all code must be compilable with C++17 or any later version of the standard.

The CI workflows are described at [`.github/workflows/README.md`](.github/workflows/README.md).

We use [Sphinx], [Doxygen], and [Breathe] for documentation and [reno] for release notes.

We prefer 100% coverage in all new code.

[Qiskit]: https://www.ibm.com/quantum/qiskit
[Sphinx]: https://www.sphinx-doc.org/
[Doxygen]: https://www.doxygen.nl/
[Breathe]: https://breathe.readthedocs.io/en/latest/
[reno]: https://docs.openstack.org/reno/
