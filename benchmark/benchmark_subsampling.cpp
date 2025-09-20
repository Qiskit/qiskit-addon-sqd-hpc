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

#include <bitset>

#include <nanobench.h>

#include "qiskit/addon/sqd/subsampling.hpp"

#include "../test/bitset_compat.hpp"

template <typename BitstringType, unsigned int N>
static void benchmark_with_bitset(ankerl::nanobench::Bench &bench)
{
    std::vector<BitstringType> bitstrings;
    for (unsigned int i = 0; i < 5; ++i) {
        BitstringType bs;
        set_bitset(N, bs, i);
        bitstrings.push_back(std::move(bs));
    }
    std::vector<double> weights{1, 2, 3, 4, 5};
    std::mt19937 rng;
    constexpr auto samples_per_batch = 4;
    for (int num_batches : {2, 10, 25}) {
        std::vector<decltype(bitstrings)> batches;
        bench.complexityN(num_batches).run("multiple_batches", [&] {
            Qiskit::addon::sqd::subsample_multiple_batches(
                batches, bitstrings, weights, samples_per_batch, num_batches, rng
            );
        });
    }
}

void benchmark_subsampling(ankerl::nanobench::Bench &bench)
{
    bench.title(std::string("Subsampling w/ std::bitset"));
    benchmark_with_bitset<std::bitset<4>, 4>(bench);

    bench.title(std::string("Subsampling w/ boost::dynamic_bitset"));
    benchmark_with_bitset<boost::dynamic_bitset<>, 4>(bench);
}
