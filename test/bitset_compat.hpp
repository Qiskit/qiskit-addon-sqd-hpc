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

#ifndef BITSET_COMPAT_HPP_
#define BITSET_COMPAT_HPP_

#include <bitset>
#include <cassert>
#include <utility>

#include <boost/dynamic_bitset.hpp>

#if !QKA_SQD_DISABLE_EXCEPTIONS

#include <bitset2.hpp>

template <std::size_t N>
static void set_bitset(unsigned int N_, Bitset2::bitset2<N> &bitset, unsigned int value)
{
    assert(N_ == N);
    bitset = Bitset2::bitset2<N>(value);
}

#endif // !QKA_SQD_DISABLE_EXCEPTIONS

static void
set_bitset(unsigned int N, boost::dynamic_bitset<> &bitset, unsigned int value)
{
    bitset = std::move(boost::dynamic_bitset<>(N, value));
}

template <std::size_t N>
static void set_bitset(unsigned int N_, std::bitset<N> &bitset, unsigned int value)
{
    assert(N_ == N);
    bitset = std::bitset<N>(value);
}

#endif // BITSET_COMPAT_HPP_
