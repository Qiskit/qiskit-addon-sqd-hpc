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

#ifndef QISKIT_ADDON_SQD_SUPPORT_BOOST_DYNAMIC_BITSET_HPP_
#define QISKIT_ADDON_SQD_SUPPORT_BOOST_DYNAMIC_BITSET_HPP_

/// Support for `boost::dynamic_bitset` (optional include)

#include "qiskit/addon/sqd/internal/bitset_common.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"

#if __has_include(<boost/dynamic_bitset.hpp>)

#include <boost/dynamic_bitset.hpp>

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

template <typename Block, typename Allocator>
struct HalfSizeImpl<boost::dynamic_bitset<Block, Allocator>> {
    using type = boost::dynamic_bitset<Block, Allocator>;
};

template <typename Block, typename Allocator>
std::array<boost::dynamic_bitset<Block, Allocator>, 2>
split_bitstring(const boost::dynamic_bitset<Block, Allocator> &bitset)
{
    if (bitset.size() % 2 != 0) {
        QKA_SQD_THROW_RUNTIME_ERROR_("Bitset size must be even");
    }
    const auto half_N = bitset.size() / 2;
    boost::dynamic_bitset<Block, Allocator> right(half_N), left(half_N);
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

#endif // _has_include("boost/dynamic_bitset.hpp")

#endif // QISKIT_ADDON_SQD_SUPPORT_BOOST_DYNAMIC_BITSET_HPP_
