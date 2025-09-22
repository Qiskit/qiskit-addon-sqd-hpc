************************************
Compilation flags
************************************

This guide documents various ``#define``\ s and other supported flags that may be passed to the compiler when using this library.

How to disable exceptions
-------------------------

To disable exceptions, the following flags must the passed to the compiler: ``-fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1``

When ``QKA_SQD_DISABLE_EXCEPTIONS`` is set to a non-zero value during compilation, the library will refrain from throwing exceptions.  Instead, if there is an error, it will send a message to ``STDERR`` and call ``std::terminate``.  The default behavior of ``std::terminate`` is to abort the program, but this behavior can be configured by calling ``std::set_terminate``.  For an MPI program, it is recommended to install a custom handler that calls ``MPI_Abort`` instead.

How to disable run-time type information (RTTI)
-----------------------------------------------

This library does not require RTTI, so the following flag can optionally be passed to the compiler to disable it: ``-fno-rtti``.

Example: passing flags to ``cmake``
-----------------------------------

Compiler flags are passed to ``cmake`` via the ``-DCMAKE_CXX_FLAGS`` option.  For instance, the test suite of this repository can be compiled without exceptions with the following commands:

.. code-block:: sh

   mkdir -p build
   cd build
   cmake .. -DCMAKE_CXX_FLAGS="-fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS"
   make

Note that compiling this repository's test suite requires that ``#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`` be set, as exceptions must be disabled in the doctest framework as well as the SQD library.

Concepts (C++20 and later)
--------------------------

This library is designed to use C++ concepts if they are available (i.e., if the code is compiled according to C++20 or a later version of the standard).  If possible, users are encouraged to use such a compiler when developing, as this will likely lead to better error messages from the compiler (e.g., when a template argument has an unexpected type).
