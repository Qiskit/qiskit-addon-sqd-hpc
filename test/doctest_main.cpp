// This code is part of Qiskit.
//
// (C) Copyright IBM 2025.
//
// This code is licensed under the Apache License, Version 2.0. You may
// obtain a copy of this license in the LICENSE.txt file in the root directory
// of this source tree or at http://www.apache.org/licenses/LICENSE-2.0.
//
// Any modifications or derivative works of this code must retain this
// copyright notice, and modified files need to carry a notice indicating
// that they have been altered from the originals.

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <mpi.h>

int main(int argc, char *argv[])
{
    // Initialize MPI
    MPI_Init(&argc, &argv);

    // Run the doctest tests without exiting
    doctest::Context context(argc, argv);
    context.setOption("no-run", false);
    context.setOption("no-exitcode", true);
    int result = context.run();

    // Finalize MPI
    MPI_Finalize();

    // Now we can exit
    return result;
}

// Special functions for when exceptions are disabled
#if QKA_SQD_DISABLE_EXCEPTIONS
#include <exception>
#include <iostream>

#include <boost/throw_exception.hpp>

// The following overloads are required (by boost) to be user-defined function
// when expections are disabled.
namespace boost
{
void throw_exception(std::exception const &e)
{
    std::cerr << "Unhandled exception during testing: " << e.what() << std::endl;
    std::terminate();
}

void throw_exception(std::exception const &e, boost::source_location const &loc)
{
    throw_exception(e);
}

} // namespace boost

#endif // QKA_SQD_DISABLE_EXCEPTIONS
