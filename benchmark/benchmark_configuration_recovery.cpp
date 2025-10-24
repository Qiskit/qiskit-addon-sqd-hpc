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

#include "../test/bitset_compat.hpp"

using Qiskit::addon::sqd::recover_configurations;

template <typename BitstringType, unsigned int N>
static void benchmark_with_bitset(ankerl::nanobench::Bench &bench)
{
    constexpr auto half_N = N / 2;
    constexpr auto num_elec_a = 10u;

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
        std::vector<BitstringType> bitstrings;
        bitstrings.reserve(num_bitstrings);
        for (int i = 0; i < num_bitstrings; ++i) {
            BitstringType bs;
            set_bitset(N, bs, i);
            bitstrings.push_back(std::move(bs));
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

void benchmark_configuration_recovery(ankerl::nanobench::Bench &bench)
{
    bench.title("Configuration recovery with std::bitset");
    benchmark_with_bitset<std::bitset<80>, 80>(bench);

    bench.title("Configuration recovery with boost::dynamic_bitset");
    benchmark_with_bitset<boost::dynamic_bitset<>, 80>(bench);

#if !QKA_SQD_DISABLE_EXCEPTIONS && !(_MSVC_LANG == 202002L)
    bench.title("Configuration recovery with Bitset2::bitset2");
    benchmark_with_bitset<Bitset2::bitset2<80>, 80>(bench);
#endif // !QKA_SQD_DISABLE_EXCEPTIONS && !(_MSVC_LANG == 202002L)
}
