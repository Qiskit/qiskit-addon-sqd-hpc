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

#ifndef QISKIT_ADDON_SQD_INTERNAL_FINITE_MATH_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_FINITE_MATH_HPP_

// This library relies on IEEE 754 semantics -- in particular, the ability to
// detect NaN and infinite values in the weight arrays it is given.  Compiling
// with `-ffast-math` (which defines `__FAST_MATH__`) breaks these semantics,
// and is not supported.
#ifdef __FAST_MATH__
#warning                                                                               \
    "qiskit-addon-sqd-hpc relies on IEEE 754 semantics and does not support -ffast-math (__FAST_MATH__)."
#endif

// `__FINITE_MATH_ONLY__` is defined to a nonzero value when the compiler has
// been told to assume that no NaN or infinite values will arise (e.g., via
// `-ffinite-math-only`, which is also implied by `-ffast-math`).  In that
// case, `std::isnan`/`std::isinf` are permitted to always return `false`, so
// any check for such values is useless and may be skipped.  A call site can
// guard such a check with `#if !QKA_SQD_FINITE_MATH_ONLY`.
//
// Note that GCC *always* defines `__FINITE_MATH_ONLY__` (to `0` by default),
// so its value must be tested rather than merely whether it is defined;
// `QKA_SQD_FINITE_MATH_ONLY` papers over that so call sites need not.
#if defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__
#define QKA_SQD_FINITE_MATH_ONLY 1
#else
#define QKA_SQD_FINITE_MATH_ONLY 0
#endif

#endif // QISKIT_ADDON_SQD_INTERNAL_FINITE_MATH_HPP_
