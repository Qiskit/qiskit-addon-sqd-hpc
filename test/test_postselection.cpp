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

#include "qiskit/addon/sqd/postselection.hpp"

#include "doctest.h"

#include <bitset>
#include <cstddef>
#include <vector>

#include <iostream>

#include <boost/dynamic_bitset.hpp>

#include "bitset_compat.hpp"

static constexpr std::size_t N = 6;

TEST_CASE_TEMPLATE(
    "Postselection", BitstringType, std::bitset<N>, boost::dynamic_bitset<>
)
{
    std::vector<std::bitset<N>> bitstrings(6);
    set_bitset(N, bitstrings[0], 0b011010); // y
    set_bitset(N, bitstrings[1], 0b100011); // n
    set_bitset(N, bitstrings[2], 0b010101); // n
    set_bitset(N, bitstrings[3], 0b010111); // n
    set_bitset(N, bitstrings[4], 0b101100); // y
    set_bitset(N, bitstrings[5], 0b100100); // n

    std::vector<double> weights = {0.1, 0.7, 0.6, 0.5, 0.3, 0.9};
    std::vector<std::bitset<6>> expected_bitstrings = {0b011010, 0b101100};
    std::vector<double> expected_weights = {0.25, 0.75};
    auto [new_bitstrings, new_weights] = Qiskit::addon::sqd::postselect_bitstrings(
        bitstrings, weights, Qiskit::addon::sqd::MatchesRightLeftHamming(1u, 2u)
    );
    CHECK(new_bitstrings == expected_bitstrings);
    CHECK(new_weights.size() == expected_weights.size());
    for (std::size_t i = 0; i < new_weights.size(); ++i) {
        CHECK(new_weights[i] == doctest::Approx(expected_weights[i]));
    }
}
