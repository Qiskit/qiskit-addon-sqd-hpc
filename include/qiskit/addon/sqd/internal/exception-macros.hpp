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

#ifndef QISKIT_ADDON_SQD_INTERNAL_EXCEPTION_MACROS_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_EXCEPTION_MACROS_HPP_

#include <exception>
#include <iostream>
#include <stdexcept>

#define QKA_SQD_NO_EXCEPTION_PREFIX_ "[qiskit-addon-sqd-hpc] "
#define QKA_SQD_NO_EXCEPTION_SUFFIX_ "\nCalling std::terminate()."

#if QKA_SQD_DISABLE_EXCEPTIONS
#define QKA_SQD_THROW_INVALID_ARGUMENT_(message)                                       \
    do {                                                                               \
        std::cerr << QKA_SQD_NO_EXCEPTION_PREFIX_ << "Invalid argument: " << (message) \
                  << QKA_SQD_NO_EXCEPTION_SUFFIX_ << std::endl;                        \
        std::terminate();                                                              \
    } while (0)
#define QKA_SQD_THROW_RUNTIME_ERROR_(message)                                          \
    do {                                                                               \
        std::cerr << QKA_SQD_NO_EXCEPTION_PREFIX_ << "Runtime error : " << (message)   \
                  << QKA_SQD_NO_EXCEPTION_SUFFIX_ << std::endl;                        \
        std::terminate();                                                              \
    } while (0)
#else
#define QKA_SQD_THROW_INVALID_ARGUMENT_(message) throw std::invalid_argument(message)
#define QKA_SQD_THROW_RUNTIME_ERROR_(message) throw std::runtime_error(message)
#endif // QKA_SQD_DISABLE_EXCEPTIONS

#endif // QISKIT_ADDON_SQD_INTERNAL_EXCEPTION_MACROS_HPP_
