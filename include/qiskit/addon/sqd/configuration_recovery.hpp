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

#ifndef QISKIT_ADDON_SQD_CONFIGURATION_RECOVERY_HPP_
#define QISKIT_ADDON_SQD_CONFIGURATION_RECOVERY_HPP_

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qiskit/addon/sqd/internal/concepts.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"
#include "qiskit/addon/sqd/internal/sample-without-replacement.hpp"

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

template <typename WeightVectorType>
void _normalize(WeightVectorType &probs)
{
    double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
    if (sum > 0.0) {
        for (double &prob : probs) {
            prob /= sum;
        }
    }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
double _p_flip_0_to_1(double ratio_exp, double occ, double eps = 0.01)
{
    // Occupancy is less than the naive expectation.
    // Flip 0s to 1 with small (<eps) probability in this case.
    if (occ < ratio_exp) {
        return occ * eps / ratio_exp;
    }

    // Occupancy is greater than or equal to the naive expectation.
    // Increase the probability to flip the bit linearly as the deviation
    // increases.
    if (ratio_exp == 1.0) {
        return eps;
    }

    double slope = (1.0 - eps) / (1.0 - ratio_exp);
    double intercept = 1.0 - slope;
    return occ * slope + intercept;
}

double _p_flip_1_to_0(double ratio_exp, double occ, double eps = 0.01)
{
    return _p_flip_0_to_1(1.0 - ratio_exp, 1.0 - occ, eps);
}
// NOLINTEND(bugprone-easily-swappable-parameters)

template <typename BitstringType>
void mask_lower_n_bits_inplace(BitstringType &bitstring, unsigned int n)
{
    assert(n <= bitstring.size());
    auto shift = bitstring.size() - n;
    bitstring <<= shift;
    bitstring >>= shift;
}

template <typename BitstringType>
constexpr BitstringType
mask_lower_n_bits(const BitstringType &bitstring, unsigned int n)
{
    BitstringType retval(bitstring);
    mask_lower_n_bits_inplace(retval, n);
    return retval;
}

template <typename BitstringType, QKA_SQD_CONCEPT_RNG_(RNGType)>
void _bipartite_bitstring_correcting(
    BitstringType &bitstring,
    const std::array<std::array<std::vector<double>, 2>, 2> &probs_table,
    std::array<std::uint64_t, 2> num_elec,
    std::pair<std::vector<std::size_t>, std::vector<double>> &scratch_vectors,
    RNGType &rng
)
{
    // Use occupancy information (via probs_table) and target Hamming weight to
    // correct a bitstring.

#if QKA_SQD_DEBUG_RECOVERY
    std::cerr << "Initial bitstring: " << bitstring << std::endl;
    std::cerr << "Desired Hamming weight: " << num_elec[0] << ' ' << num_elec[1]
              << std::endl;
#endif

    // The number of bits should be even - this was already checked in the calling
    // function
    const auto partition_size = probs_table[0][0].size();

    // Determine starting Hamming weights
    std::array<std::uint64_t, 2> initial_hamming_weight;
    const auto n_right = mask_lower_n_bits(bitstring, partition_size).count();
    initial_hamming_weight[0] = n_right;
    initial_hamming_weight[1] = bitstring.count() - n_right;

    // Handle RIGHT (alpha) then LEFT (beta) bits
    std::uint64_t offset = 0;
    for (int s = 0; s < 2; ++s) {
        if (initial_hamming_weight[s] != num_elec[s]) {
            const bool flip = bool(
                initial_hamming_weight[s] > num_elec[s]
            ); // 1 or 0 depending on which should be flipped
            const std::uint64_t num_flip = std::abs(
                static_cast<int>(initial_hamming_weight[s]) -
                static_cast<int>(num_elec[s])
            );
            auto &[indices, weights] = scratch_vectors;
            indices.clear();
            weights.clear();
            for (std::uint64_t j = 0; j < partition_size; ++j) {
                if (bitstring[j + offset] == flip) {
                    indices.push_back(j + offset);
                    weights.push_back(probs_table[s][flip][j]);
                }
            }
            internal::NoReplacementSampler sampler(weights);
            for (std::size_t i = 0; i < num_flip; ++i) {
                const auto idx = indices[sampler(rng)];
                bitstring.flip(idx);
            }
        }
        offset += partition_size;
    }

#if QKA_SQD_DEBUG_RECOVERY
    std::cerr << "Final bitstring: " << bitstring << '\n' << std::endl;
#endif
    assert(mask_lower_n_bits(bitstring, partition_size).count() == num_elec[0]);
    assert(bitstring.count() == num_elec[0] + num_elec[1]);
}

} // namespace internal

/// Refine bitstrings based on average orbital occupancy and a target
/// Hamming weight.
///
/// @param[in] bitstrings A container (e.g., `std::vector`) of bitstrings.
/// @param[in] probabilities A 1D array specifying a probability distribution over
///     the bitstrings.  Must contain the same number of elements as `bitstrings`.
/// @param[in] avg_occupancies Size-2 `std::array` of `std::vector<double>`s holding the
///     mean occupancy of the spin-up and spin-down orbitals, respectively.  Each
///     vector's size must be half the size of a single bitstring.
/// @param[in] num_elec Size-2 `std::array` containing the number of spin-up and
///     spin-down electrons in the system, respectively.
/// @param[in,out] rng Random number generator.
///
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam RNGType Type of random number generator.
///
/// @return A refined `std::vector` of unique bitstrings and a parallel, updated
///     probability array.
template <
    typename BitstringVectorType, typename WeightVectorType,
    QKA_SQD_CONCEPT_RNG_(RNGType)>
[[nodiscard]] std::pair<BitstringVectorType, WeightVectorType> recover_configurations(
    const BitstringVectorType &bitstrings, const WeightVectorType &probabilities,
    const std::array<std::vector<double>, 2> &avg_occupancies,
    std::array<std::uint64_t, 2> num_elec, RNGType &rng
)
{
    if (bitstrings.size() != probabilities.size()) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Probabilities vector must have length that matches the bitstrings vector."
        );
    }

    const auto partition_size = avg_occupancies[0].size();
    if (avg_occupancies[1].size() != partition_size) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Average occupancies vectors must have matching number of alpha and beta "
            "orbitals."
        );
    }
    if (num_elec[0] > partition_size || num_elec[1] > partition_size) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Desired Hamming weight cannot be larger than the number of orbitals."
        );
    }

    // Populate the probabilities table
    std::array<std::array<std::vector<double>, 2>, 2> probs_table;
    for (int s = 0; s < 2; ++s) {
        probs_table[s][0].resize(partition_size);
        probs_table[s][1].resize(partition_size);
        // NOLINTBEGIN(bugprone-narrowing-conversions)
        double density_s = static_cast<double>(num_elec[s]) / partition_size;
        // NOLINTEND(bugprone-narrowing-conversions)
        for (std::size_t i = 0; i < partition_size; ++i) {
            const auto occ = avg_occupancies[s][i];
            probs_table[s][0][i] = internal::_p_flip_0_to_1(density_s, occ);
            probs_table[s][1][i] = internal::_p_flip_1_to_0(density_s, occ);
        }
    }

    using BitstringType = typename BitstringVectorType::value_type;
    std::unordered_map<BitstringType, double> corrected_dict;

    std::pair<std::vector<std::size_t>, std::vector<double>> scratch_vectors;
    for (std::size_t i = 0; i < bitstrings.size(); ++i) {
        const auto &bitstring = bitstrings[i];
        if (bitstring.size() != 2 * partition_size) {
            QKA_SQD_THROW_INVALID_ARGUMENT_(
                "Bitstring length must be twice the number of orbitals."
            );
        }

        // Correct the bitstring
        auto corrected_bitstring = bitstring;
        internal::_bipartite_bitstring_correcting(
            corrected_bitstring, probs_table, num_elec, scratch_vectors, rng
        );

        // Use the unordered_map to remove duplicates
        const auto freq = probabilities[i];
        corrected_dict[corrected_bitstring] += freq;
    }

    BitstringVectorType bitstrings_out;
    WeightVectorType freqs_out;

    for (const auto &[bitstring, freq] : corrected_dict) {
        bitstrings_out.emplace_back(bitstring);
        freqs_out.push_back(freq);
    }

    // Normalize the frequencies
    internal::_normalize(freqs_out);

    return {bitstrings_out, freqs_out};
}

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_CONFIGURATION_RECOVERY_HPP_
