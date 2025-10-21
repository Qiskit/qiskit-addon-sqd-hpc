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

#include "doctest.h"
#include "qiskit/addon/sqd/configuration_recovery.hpp"

#include <array>
#include <bitset>
#include <random>
#include <utility>
#include <vector>

#include "bitset_compat.hpp"

using Qiskit::addon::sqd::recover_configurations;

#if !QKA_SQD_DISABLE_EXCEPTIONS
#define BITSET2_IF_AVAILABLE , Bitset2::bitset2<4>
#else
#define BITSET2_IF_AVAILABLE
#endif

TEST_CASE_TEMPLATE(
    "Configuration recovery", BitstringType, std::bitset<4>,
    boost::dynamic_bitset<> BITSET2_IF_AVAILABLE
)
{
    constexpr auto N = 4;
    std::mt19937_64 rng;
    std::vector<BitstringType> bitstrings;
    for (unsigned int i = 0; i < 5; ++i) {
        BitstringType bs;
        set_bitset(N, bs, i);
        bitstrings.push_back(bs);
    }
    std::vector<double> probabilities{1, 2, 3, 4, 5};
    std::vector<double> avg_occupancies{0.1, 0.2};
    unsigned int num_elec_a = 1, num_elec_b = 1;

    std::ignore = recover_configurations(
        bitstrings, probabilities, {avg_occupancies, avg_occupancies},
        {num_elec_a, num_elec_b}, rng
    );
    CHECK(true); // FIXME
}

TEST_CASE("Configuration recovery tests from python addon")
{
    // https://github.com/Qiskit/qiskit-addon-sqd/blob/main/test/test_configuration_recovery.py
    std::mt19937_64 rng;
    SUBCASE("Empty test")
    {
        constexpr auto num_orbs = 6;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 0;
        constexpr auto ham_l = 1;
        const std::vector<std::bitset<num_orbs>> empty_bitstring_vec;
        const std::vector<double> empty_probs;
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs), std::vector<double>(half_orbs)
        };
        auto [mat_rec, probs_rec] = recover_configurations(
            empty_bitstring_vec, empty_probs, occs, {ham_r, ham_l}, rng
        );
        CHECK(mat_rec.size() == 0);
        CHECK(probs_rec.size() == 0);
    }
    SUBCASE("Basic test. Zeros to ones.")
    {
        constexpr auto num_orbs = 4;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 2;
        constexpr auto ham_l = 2;
        const std::vector<std::bitset<num_orbs>> bitstrings(1);
        const std::vector<double> probs(1, 1.0);
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs, 1.000001),
            std::vector<double>(half_orbs, 1.0)
        };
        auto [mat_rec, probs_rec] =
            recover_configurations(bitstrings, probs, occs, {ham_r, ham_l}, rng);
        CHECK(mat_rec.size() == 1);
        CHECK(mat_rec[0] == 0b1111);
        CHECK(probs_rec.size() == 1);
        CHECK(probs_rec[0] == 1.0);
    }
    SUBCASE("Basic test. Ones to zeros.")
    {
        constexpr auto num_orbs = 4;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 0;
        constexpr auto ham_l = 0;
        const std::vector<std::bitset<num_orbs>> bitstrings(1, 0b1111);
        const std::vector<double> probs(1, 1.0);
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs, -1e6), std::vector<double>(half_orbs)
        };
        auto [mat_rec, probs_rec] =
            recover_configurations(bitstrings, probs, occs, {ham_r, ham_l}, rng);
        CHECK(mat_rec.size() == 1);
        CHECK(mat_rec[0] == 0);
        CHECK(probs_rec.size() == 1);
        CHECK(probs_rec[0] == 1.0);
    }
    SUBCASE("Basic test. Mismatching orbitals.")
    {
        constexpr auto num_orbs = 4;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 0;
        constexpr auto ham_l = 1;
        const std::vector<std::bitset<num_orbs>> bitstrings(1, 0b1111);
        const std::vector<double> probs(1, 1.0);
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs), std::vector<double>(half_orbs)
        };
        occs[1][0] = 1.0;
        auto [mat_rec, probs_rec] =
            recover_configurations(bitstrings, probs, occs, {ham_r, ham_l}, rng);
        CHECK(mat_rec.size() == 1);
        CHECK(mat_rec[0] == 0b0100);
        CHECK(probs_rec.size() == 1);
        CHECK(probs_rec[0] == 1.0);
    }
    SUBCASE("Bad Hamming right")
    {
        constexpr auto num_orbs = 4;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 3;
        constexpr auto ham_l = 1;
        const std::vector<std::bitset<num_orbs>> bitstrings(1);
        const std::vector<double> probs(1, 1.0);
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs, 1.0), std::vector<double>(half_orbs, 1.0)
        };
        CHECK_THROWS_AS(
            std::ignore =
                recover_configurations(bitstrings, probs, occs, {ham_r, ham_l}, rng),
            std::invalid_argument
        );
    }
    SUBCASE("Bad Hamming left")
    {
        constexpr auto num_orbs = 4;
        constexpr auto half_orbs = num_orbs / 2;
        constexpr auto ham_r = 2;
        constexpr auto ham_l = 3;
        const std::vector<std::bitset<num_orbs>> bitstrings(1);
        const std::vector<double> probs(1, 1.0);
        std::array<std::vector<double>, 2> occs{
            std::vector<double>(half_orbs, 1.0), std::vector<double>(half_orbs, 1.0)
        };
        CHECK_THROWS_AS(
            std::ignore =
                recover_configurations(bitstrings, probs, occs, {ham_r, ham_l}, rng),
            std::invalid_argument
        );
    }
}

TEST_CASE_TEMPLATE(
    "Bit manipulation", BitstringType, std::bitset<7>, boost::dynamic_bitset<>
)
{
    constexpr std::size_t N = 7;
    BitstringType bs, bs_expected;
    set_bitset(N, bs, 93);
    set_bitset(N, bs_expected, 5);
    Qiskit::addon::sqd::internal::mask_lower_n_bits_inplace(bs, 3);
    CHECK(bs == bs_expected);
}
