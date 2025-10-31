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

#include "qiskit/addon/sqd/eigensolver/sbd.hpp"

#include <bitset>
#include <unordered_set>
#include <vector>

#include "bitset_compat.hpp"

#include "doctest.h"

TEST_CASE("Basic SBD test")
{
    auto sbd_data = sbd::tpb::generate_sbd_data(0, nullptr);
    auto fcidump = sbd::LoadFCIDump("data/fcidump_mock.txt");
    std::vector<std::bitset<4>> adets{3, 5, 6};
    auto result =
        Qiskit::addon::sqd::sbd_solve(MPI_COMM_WORLD, sbd_data, fcidump, adets, adets);
}
