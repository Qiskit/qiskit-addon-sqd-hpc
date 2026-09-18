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

#ifndef QISKIT_ADDON_SQD_INTERNAL_DENSE_MAP_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_DENSE_MAP_HPP_

// Thin alias over the vendored ankerl::unordered_dense hash map, exposed under
// a project-internal name so the rest of the codebase never references the
// upstream `ankerl` namespace directly.  The vendored headers under
// internal/vendor/ are kept byte-for-byte identical to upstream to make
// re-vendoring a pure copy; all project-owned adaptation lives here.

#include "qiskit/addon/sqd/internal/vendor/unordered_dense/unordered_dense.h"

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

/// A flat (open-addressed, densely stored) hash map used to remove duplicates
/// efficiently.  It significantly outperforms `std::unordered_map` when the
/// number of distinct keys is large, which is the common case for the
/// configuration-recovery dedup step.
template <
    typename Key, typename T, typename Hash = vendored::unordered_dense::hash<Key>,
    typename KeyEqual = std::equal_to<Key>
>
using dense_map = vendored::unordered_dense::map<Key, T, Hash, KeyEqual>;

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_INTERNAL_DENSE_MAP_HPP_
