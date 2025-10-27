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

#ifndef QISKIT_ADDON_SQD_FERMION_HPP_
#define QISKIT_ADDON_SQD_FERMION_HPP_

/// Tools for studying fermionic systems

#include <algorithm>
#include <array>
#include <climits>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qiskit/addon/sqd/bitset_common.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

} // namespace internal

/// Convert bitstrings into CI strings (representations of determinants).
///
/// This function separates each bitstring in bitstring_matrix in half, combining the
/// right and left halves of all the bitstrings into a single set of unique
/// configurations.
///
/// @param[in] bitstrings Population of bitstrings.  @param[in] max_dimension
/// Maximum dimension of returned CI strings.  If less than the number of CI
///     strings, the list of CI strings will be truncated.
/// @param[in] include_configurations A list of CI strings that will be included in the
///     output, regardless of whether they are contained in \p bitstrings.
///
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
template <class BitstringVectorType>
auto bitstrings_to_ci_strings_symmetrize_spin(
    const BitstringVectorType &bitstrings,
    std::optional<unsigned int> max_dimension = std::nullopt,
    std::optional<std::reference_wrapper<const std::vector<
        internal::HalfSize<typename BitstringVectorType::value_type>>>>
        include_configurations = std::nullopt
) -> std::vector<internal::HalfSize<typename BitstringVectorType::value_type>>
{
    using BitstringType = typename BitstringVectorType::value_type;
    using HalfBitstringType = internal::HalfSize<BitstringType>;
    if (bitstrings.empty()) {
        return std::vector<HalfBitstringType>();
    }
    if (bitstrings[0].size() % 2 != 0) {
        QKA_SQD_THROW_INVALID_ARGUMENT_("Bitstring length must be even");
    }
    const auto norb = bitstrings[0].size() / 2;

    std::unordered_map<HalfBitstringType, unsigned int> counts;
    // Include any CI strings that are being explicitly included
    if (include_configurations) {
        for (const auto &ci_string : include_configurations->get()) {
            // Add a large constant, which is larger than any existing count
            counts[ci_string] += bitstrings.size();
        }
    }
    // For each bitstrings, separate into CI strings
    for (const auto &bitstring : bitstrings) {
        if (bitstring.size() != 2 * norb) {
            QKA_SQD_THROW_INVALID_ARGUMENT_("Bitstrings must have uniform length");
        }
        auto [right_ci, left_ci] = internal::split_bitstring(bitstring);
        ++counts[right_ci];
        ++counts[left_ci];
    }

    // Sort them by count, largest first
    std::vector<std::pair<unsigned int, HalfBitstringType>> by_counts;
    by_counts.reserve(counts.size());
    for (const auto &[ci_string, count] : counts) {
        by_counts.emplace_back(count, std::move(ci_string));
    }
    std::sort(by_counts.begin(), by_counts.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });

    // Truncate if max_dimension is given
    if (max_dimension && by_counts.size() > *max_dimension) {
        by_counts.resize(*max_dimension);
    }

    // Populate return value
    std::vector<HalfBitstringType> retval;
    retval.reserve(by_counts.size());
    for (const auto &[_, ci_string] : by_counts) {
        retval.push_back(std::move(ci_string));
    }
    return retval;
}

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_FERMION_HPP_
