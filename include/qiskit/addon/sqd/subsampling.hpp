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

#ifndef QISKIT_ADDON_SQD_SUBSAMPLING_HPP_
#define QISKIT_ADDON_SQD_SUBSAMPLING_HPP_

/// Subsampling routines

#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
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

/// Subsample a single batch of bitstrings (mutating version)
///
/// This version can be useful if you want to avoid reallocation by re-using an
/// existing batch vector.
///
/// Note: You must de-duplicate the bitstrings before calling this, otherwise
/// you may get duplicate bitstrings in the output.
///
/// @param[out] batch This will be cleared and overwritten with the subsampled
///     bitstrings.
/// @param[in] bitstrings Population of bitstrings.
/// @param[in] weights Relative weight of each bitstring (need not be normalized to 1).
///     Must be the same length as \p bitstrings and contain only non-negative values.
/// @param[in] samples_per_batch Number of samples to return in \p batch.
///     Cannot be greater than the number of bitstrings.
/// @param[in,out] rng Random number generator to use for sampling.
///
/// @tparam BatchVectorType Type of `batch`, must be compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam RNGType Type of random number generator.
template <
    typename BatchVectorType, typename BitstringVectorType, typename WeightVectorType,
    QKA_SQD_CONCEPT_RNG_(RNGType)>
void subsample(
    BatchVectorType &batch, const BitstringVectorType &bitstrings,
    const WeightVectorType &weights, unsigned int samples_per_batch, RNGType &rng
)
{
    if (bitstrings.size() != weights.size()) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Weights vector must match the number of bitstrings"
        );
    }
    // This test is technically covered below, but we might as well bail early,
    // with a more accurate error message, if it is true.
    if (samples_per_batch > bitstrings.size()) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Cannot draw more samples than number of bitstrings"
        );
    }

    internal::NoReplacementSampler sampler(weights);
    if (samples_per_batch > sampler.get_remaining_nonzero_weights()) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "Cannot draw more samples than number of "
            "bitstrings with nonzero weight"
        );
    }

    batch.clear();
    batch.reserve(samples_per_batch);

    while (batch.size() < samples_per_batch) {
        const auto idx = sampler(rng);
        batch.push_back(bitstrings[idx]);
    }
}

/// Subsample a single batch of bitstrings
///
/// Note: You must de-duplicate the bitstrings before calling this, otherwise
/// you may get duplicate bitstrings in the output.
///
/// @param[in] bitstrings Population of bitstrings.
/// @param[in] weights Relative weight of each bitstring (need not be normalized to 1).
///     Must be the same length as \p bitstrings and contain only non-negative values.
/// @param[in] samples_per_batch Number of samples to return in \p batch.
///     Cannot be greater than the number of bitstrings.
/// @param[in,out] rng Random number generator to use for sampling.
///
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam RNGType Type of random number generator.
///
/// @return The subsampled bitstrings.
template <
    typename BitstringVectorType, typename WeightVectorType,
    QKA_SQD_CONCEPT_RNG_(RNGType)>
BitstringVectorType subsample(
    const BitstringVectorType &bitstrings, const WeightVectorType &weights,
    unsigned int samples_per_batch, RNGType &rng
)
{
    BitstringVectorType batch;
    subsample(batch, bitstrings, weights, samples_per_batch, rng);
    return batch;
}

/// Subsample multiple batches of bitstrings (mutating version)
///
/// This version can be useful if you want to avoid reallocation by re-using
/// existing batch vectors.
///
/// Note: You must de-duplicate the bitstrings before calling this, otherwise
/// you may get duplicate bitstrings in the output.
///
/// @param[out] batches This will be overwritten with the batches of subsampled
///     bitstrings.
/// @param[in] bitstrings Population of bitstrings.
/// @param[in] weights Relative weight of each bitstring (need not be normalized to 1).
///     Must be the same length as \p bitstrings and contain only non-negative values.
/// @param[in] samples_per_batch Number of samples to return per batch.
///     Cannot be greater than the number of bitstrings.
/// @param[in] num_batches Number of batches.
/// @param[in,out] rng Random number generator to use for sampling.
///
/// @tparam BatchesVectorType Type of `batches`, must be compatible with
///     `std::vector<std::vector<boost::dynamic_bitset<>>>`.
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam RNGType Type of random number generator.
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
template <
    typename BatchesVectorType, typename BitstringVectorType, typename WeightVectorType,
    QKA_SQD_CONCEPT_RNG_(RNGType)>
void subsample_multiple_batches(
    BatchesVectorType &batches, const BitstringVectorType &bitstrings,
    const WeightVectorType &weights, unsigned int samples_per_batch,
    unsigned int num_batches, RNGType &rng
)
{
    batches.resize(num_batches);
    for (decltype(num_batches) i = 0; i < num_batches; ++i) {
        subsample(batches[i], bitstrings, weights, samples_per_batch, rng);
    }
}

/// Subsample multiple batches of bitstrings
///
/// Note: You must de-duplicate the bitstrings before calling this, otherwise
/// you may get duplicate bitstrings in the output.
///
/// @param[in] bitstrings Population of bitstrings.
/// @param[in] weights Relative weight of each bitstring (need not be normalized to 1).
///     Must be the same length as \p bitstrings and contain only non-negative values.
/// @param[in] samples_per_batch Number of samples to return per batch.
///     Cannot be greater than the number of bitstrings.
/// @param[in] num_batches Number of batches.
/// @param[in,out] rng Random number generator to use for sampling.
///
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam RNGType Type of random number generator.
///
/// @return The batches of subsampled bitstrings.
template <
    typename BitstringVectorType, typename WeightVectorType,
    QKA_SQD_CONCEPT_RNG_(RNGType)>
std::vector<BitstringVectorType> subsample_multiple_batches(
    const BitstringVectorType &bitstrings, const WeightVectorType &weights,
    unsigned int samples_per_batch, unsigned int num_batches, RNGType &rng
)
{
    std::vector<BitstringVectorType> batches;
    subsample_multiple_batches(
        batches, bitstrings, weights, samples_per_batch, num_batches, rng
    );
    return batches;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_SUBSAMPLING_HPP_
