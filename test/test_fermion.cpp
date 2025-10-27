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

#include "doctest.h"

TEST_CASE("Bitstrings to CI strings")
{
    std::vector<std::bitset<6>> bitstrings{{0b011011, 0b101011}};
    const auto ci_strings =
        Qiskit::addon::sqd::bitstrings_to_ci_strings_symmetrize_spin(bitstrings);
    std::unordered_set<std::bitset<3>> expected{{0b011, 0b101}};
    for (const auto &ci_string : ci_strings) {
        CHECK(ci_string.size() == 3);
        CHECK(expected.erase(ci_string));
    }
    CHECK(expected.empty());
}
