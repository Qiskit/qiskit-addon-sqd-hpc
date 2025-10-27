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

#ifndef QISKIT_ADDON_SQD_BITSET_COMMON_HPP_
#define QISKIT_ADDON_SQD_BITSET_COMMON_HPP_

/// Interfaces/utilities for supporting a variety of bitset types.

#include <array>
#include <bitset>
#include <cstddef>

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

template <typename T>
struct HalfSizeImpl;

template <std::size_t N>
struct HalfSizeImpl<std::bitset<N>> {
    static_assert(N % 2 == 0, "N must be even");
    using type = std::bitset<N / 2>;
};

template <typename T>
using HalfSize = typename HalfSizeImpl<T>::type;

template <std::size_t N>
std::array<HalfSize<std::bitset<N>>, 2> split_bitstring(const std::bitset<N> &bitset)
{
    constexpr auto half_N = N / 2;
    HalfSize<std::bitset<N>> right, left;
    for (std::size_t i = 0; i < half_N; ++i) {
        right[i] = bitset[i];
        left[i] = bitset[i + half_N];
    }
    return {right, left};
}

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_BITSET_COMMON_HPP_
