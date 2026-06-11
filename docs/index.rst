#################################################
Sample-based quantum diagonalization (HPC-ready)
#################################################

This repository contains an HPC-ready implementation of the `Qiskit addon for sample-based qantum diagonalization (SQD) <https://github.com/Qiskit/qiskit-addon-sqd>`__.

Unlike the existing Python addon, this code is written in modern C++17 and is designed to enable HPC workflows and applications, particularly those that require a single binary to be compiled for use with MPI.  This library builds on existing interfaces in the C++ standard template library (STL); specifically, this code code base has the following characteristics:

- You can use it to leverage any pseudo-random number generator compatible with the ``<random>`` header.
- It relies on standard interfaces for storing bitstrings, namely ``std::bitset`` and ``boost::dynamic_bitset``, which are aligned with the `Qiskit bit ordering conventions <https://quantum.cloud.ibm.com/docs/guides/bit-ordering>`__ (c.f. `to_ulong <https://cppreference.com/w/cpp/utility/bitset/to_ulong.html>`__ and `to_string <https://cppreference.com/w/cpp/utility/bitset/to_string.html>`__).
- It is compatible with STL containers, including ``std::vector``.

The code is cross-platform, fully tested, fully documented, and contains an integrated benchmark suite.

Table of Contents
-----------------

.. toctree::
   :maxdepth: 2

   Documentation Home <self>
   Compilation flags <compilation-flags>
   API Reference <apidocs/index>
   GitHub <https://github.com/Qiskit/qiskit-addon-sqd-hpc>
   Release Notes <release-notes>
