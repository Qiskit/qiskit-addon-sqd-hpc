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

#include "qiskit/addon/sqd/subsampling.hpp"

#include "doctest.h"

#include <bitset>
#include <unordered_set>
#include <vector>

#include <boost/dynamic_bitset.hpp>

#include "bitset_compat.hpp"

TEST_CASE_TEMPLATE(
    "Subsample batches", BitstringType, std::bitset<4>, boost::dynamic_bitset<>
)
{
    constexpr unsigned int N = 4;
    std::vector<BitstringType> bitstrings;
    for (unsigned int i = 0; i < 5; ++i) {
        BitstringType bs;
        set_bitset(N, bs, i);
        bitstrings.push_back(bs);
    }
    std::vector<double> weights{1, 2, 3, 4, 5};
    std::mt19937 rng;
    constexpr auto samples_per_batch = 4;
    constexpr auto num_batches = 2;
    auto batches = Qiskit::addon::sqd::subsample_multiple_batches(
        bitstrings, weights, samples_per_batch, num_batches, rng
    );
    CHECK(batches.size() == num_batches);
    for (const auto &batch : batches) {
        CHECK(batch.size() == samples_per_batch);
        std::unordered_set<BitstringType> bitstrings_drawn;
        for (const auto &bitstring : batch) {
            // Check that bitstring is not a duplicate
            auto insert_retval = bitstrings_drawn.insert(bitstring);
            CHECK(insert_retval.second);
            // Check that it's a bitstring we expect to see
            CHECK(bitstring.to_ulong() < 5);
            // Check the size, for sanity
            CHECK(bitstring.size() == N);
        }
    }
}
