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

#ifndef QISKIT_ADDON_SQD_INTERNAL_CONCEPTS_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_CONCEPTS_HPP_

#if !defined(QKA_SQD_USE_CONCEPTS)
// The user did not specify a preference, so choose behavior based on C++
// version
#if __cplusplus >= 202002L
#define QKA_SQD_USE_CONCEPTS 1
#else
#define QKA_SQD_USE_CONCEPTS 0
#endif
#endif // !defined(QKA_SQD_USE_CONCEPTS)

#if QKA_SQD_USE_CONCEPTS
#include <random>
#define QKA_SQD_CONCEPT_RNG_(T) std::uniform_random_bit_generator T
#else
#define QKA_SQD_CONCEPT_RNG_(T) typename T
#endif

#endif // QISKIT_ADDON_SQD_INTERNAL_CONCEPTS_HPP_
