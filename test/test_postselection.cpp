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

TEST_CASE("Postselection")
{
    std::vector<std::bitset<6>> bitstrings = {
        0b011010, // y
        0b100011, // n
        0b010101, // n
        0b010111, // n
        0b101100, // y
        0b100100  // n
    };
    std::vector<double> weights = {0.1, 0.7, 0.6, 0.5, 0.3, 0.9};
    std::vector<std::bitset<6>> expected_bitstrings = {0b011010, 0b101100};
    std::vector<double> expected_weights = {0.25, 0.75};
    auto [new_bitstrings, new_weights] = Qiskit::addon::sqd::postselect_bitstrings(
        bitstrings, weights, Qiskit::addon::sqd::MatchesRightLeftHamming(1, 2)
    );
    CHECK(new_bitstrings == expected_bitstrings);
    CHECK(new_weights.size() == expected_weights.size());
    for (std::size_t i = 0; i < new_weights.size(); ++i) {
        CHECK(new_weights[i] == doctest::Approx(expected_weights[i]));
    }
}
