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

#include <array>
#include <bitset>
#include <random>
#include <utility>
#include <vector>

#include <nanobench.h>

#include "qiskit/addon/sqd/configuration_recovery.hpp"

using Qiskit::addon::sqd::recover_configurations;

void benchmark_configuration_recovery(ankerl::nanobench::Bench &bench)
{
    bench.title("Configuration recovery");

    constexpr auto half_N = 40;
    constexpr auto N = 2 * half_N;
    constexpr auto num_elec_a = 10u;

    using Bitstring = std::bitset<N>;

    std::mt19937_64 rng;
    std::uniform_real_distribution<double> dis(0.0, 1.0);

    // Populate random occupancies
    std::array<std::vector<double>, 2> avg_occupancies;
    for (int s = 0; s < 2; ++s) {
        avg_occupancies[s].reserve(half_N);
        for (int i = 0; i < half_N; ++i) {
            avg_occupancies[s].push_back(dis(rng));
        }
    }

    for (int num_bitstrings : {10, 100, 1000, 10000, 100000}) {
        // Populate some sequential bitstrings
        std::vector<Bitstring> bitstrings;
        bitstrings.reserve(num_bitstrings);
        for (int i = 0; i < num_bitstrings; ++i) {
            bitstrings.emplace_back(i);
        }

        // Populate random probabilities
        std::vector<double> probabilities;
        probabilities.reserve(bitstrings.size());
        for (const auto &_ : bitstrings) {
            probabilities.push_back(dis(rng));
        }

        // Perform benchmaking
        bench.complexityN(num_bitstrings).run("configuration_recovery", [&] {
            std::ignore = recover_configurations(
                bitstrings, probabilities, avg_occupancies, {num_elec_a, num_elec_a},
                rng
            );
        });
    }
}
