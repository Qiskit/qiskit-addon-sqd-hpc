#############################
Qiskit addon: SQD (HPC-ready)
#############################

This repository contains an HPC-ready implementation of the `Qiskit addon for Sample-based Quantum Diagonalization (SQD) <https://github.com/Qiskit/qiskit-addon-sqd>`__.

Unlike the existing Python addon, this code is written in modern C++17 and is designed to enable HPC workflows and applications, particularly those that require a single binary to be compiled for use with MPI.  This library builds upon existing interfaces in the C++ standard template library (STL); specifically, this code-base is:

- Able to leverage any pseudo-random number generator compatible with the ``<random>`` header.
- Reliant on standard interfaces for storing bitstrings, namely ``std::bitset`` and ``boost::dynamic_bitset``, which are themselves aligned with `Qiskit's bit-ordering conventions <https://quantum.cloud.ibm.com/docs/en/guides/bit-ordering>`__ (c.f. `to_ulong <https://en.cppreference.com/w/cpp/utility/bitset/to_ulong.html>`__ and `to_string <https://en.cppreference.com/w/cpp/utility/bitset/to_string.html>`__).
- Compatible with STL containers, including ``std::vector``.

The code is cross-platform, fully tested, fully documented, and contains an integrated benchmark suite.

Table of Contents
-----------------

.. toctree::

   Compilation flags <compilation-flags>
   API Reference <apidocs/index>
