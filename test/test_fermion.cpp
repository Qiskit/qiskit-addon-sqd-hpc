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

#include "qiskit/addon/sqd/fermion.hpp"

#include <bitset>
#include <unordered_set>
#include <vector>

#include "bitset_compat.hpp"

#include "doctest.h"

using Qiskit::addon::sqd::internal::HalfSize;

TEST_CASE_TEMPLATE(
    "Bitstrings to CI strings", BitstringType, std::bitset<6>, boost::dynamic_bitset<>
)
{
    constexpr unsigned int N = 6;
    std::vector<BitstringType> bitstrings;
    BitstringType bs;
    set_bitset(N, bs, 0b011011);
    bitstrings.push_back(std::move(bs));
    set_bitset(N, bs, 0b101011);
    bitstrings.push_back(std::move(bs));
    const auto ci_strings =
        Qiskit::addon::sqd::bitstrings_to_ci_strings_symmetrize_spin(bitstrings);
    std::unordered_set<HalfSize<BitstringType>> expected;
    HalfSize<BitstringType> bs2;
    set_bitset(N / 2, bs2, 0b011);
    expected.insert(std::move(bs2));
    set_bitset(N / 2, bs2, 0b101);
    expected.insert(std::move(bs2));
    for (const auto &ci_string : ci_strings) {
        CHECK(ci_string.size() == 3);
        CHECK(expected.erase(ci_string));
    }
    CHECK(expected.empty());
}
