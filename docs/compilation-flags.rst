************************************
Compilation flags
************************************

This guide describes the compiler flags supported by this library, including ``#define``-based macros.

How to disable exceptions
-------------------------

To disable exceptions, the following flags must the passed to the compiler: ``-fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1``

When ``QKA_SQD_DISABLE_EXCEPTIONS`` is set to a non-zero value during compilation, the library will not throw exceptions.  Instead, if there is an error, it will send a message to ``STDERR`` and call ``std::terminate``.  The default behavior of ``std::terminate`` is to abort the program, but this behavior can be configured by calling ``std::set_terminate``.  For an MPI program, it is recommended to install a custom handler that calls ``MPI_Abort`` instead.

How to disable run-time type information (RTTI)
-----------------------------------------------

This library does not require RTTI, so the following flag can optionally be passed to the compiler to disable it: ``-fno-rtti``.

Example: Pass flags to ``cmake``
-----------------------------------

Compiler flags are passed to ``cmake`` by using the ``-DCMAKE_CXX_FLAGS`` option.  For instance, the test suite of this repository can be compiled without exceptions with the following commands:

.. code-block:: sh

   mkdir -p build
   cd build
   cmake .. -DCMAKE_CXX_FLAGS="-fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS"
   make

Note that compiling this repository's test suite requires that you set ``#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS``, as exceptions must be disabled in the doctest framework and the SQD library.

A note on ``-ffast-math``
-------------------------

This library relies on IEEE 754 floating-point semantics.  **The use of** ``-ffast-math`` **is not supported**, and the library will emit a compiler warning if it detects that ``__FAST_MATH__`` is defined.

The related flag ``-ffinite-math-only`` (which is also implied by ``-ffast-math``) is a narrower assertion that no NaN or infinite values arise.  When it is in effect (i.e., when ``__FINITE_MATH_ONLY__`` is defined to a nonzero value), the library omits the now-redundant NaN and infinity checks but is otherwise expected to behave correctly.

How to enable OpenMP
--------------------

This library is header-only and contains OpenMP-parallelized code paths that are compiled only when the consumer enables OpenMP (for example, ``-fopenmp`` with GCC or Clang).  No library-specific macro is needed: the parallel paths are guarded by the ``_OPENMP`` macro that OpenMP-capable compilers predefine, and a serial build results if OpenMP is absent.

Compiler support
~~~~~~~~~~~~~~~~

The parallel paths are tested with GCC and Clang.  They require an OpenMP implementation that accepts an unsigned loop index in a worksharing construct, which the OpenMP 3.0 specification introduced and which these loops rely on to iterate over a container.

Microsoft Visual C++'s ``/openmp`` switch implements OpenMP 2.0, which does not allow an unsigned index, so building the parallel paths with it is **not supported**.  MSVC's `/openmp:llvm <https://learn.microsoft.com/en-us/cpp/build/reference/openmp-enable-openmp-2-0-support>`__ switch does accept unsigned indices per OpenMP 3.0, but Microsoft documents it as experimental and not available for production code, and it is untested here.  Building with MSVC and no OpenMP switch is fully supported and yields the serial paths, as with any other compiler without OpenMP.

Enabling OpenMP can affect the numerical results of routines that consume randomness, because the parallel code paths draw from per-work-item random streams rather than sequentially from the caller's generator.  With a counter-based generator the results are unchanged.  With any other generator they are unchanged on one thread and depend on the thread count beyond that.  See :doc:`random-number-generation` for what is and is not reproducible.

Concepts (C++20 and later)
--------------------------

This library is designed to use C++ concepts if they are available (that is, if the code is compiled according to C++20 or a later version of the standard).  If possible, users are encouraged to use such a compiler when developing, as this will likely lead to better error messages from the compiler (such as when a template argument has an unexpected type).
