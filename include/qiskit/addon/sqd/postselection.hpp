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

#ifndef QISKIT_ADDON_SQD_POSTSELECTION_HPP_
#define QISKIT_ADDON_SQD_POSTSELECTION_HPP_

#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "qiskit/addon/sqd/internal/exception-macros.hpp"

// QKA_SQD_IF_UNLIKELY_ private macro
#if __cplusplus >= 202002L
#define QKA_SQD_IF_UNLIKELY_(condition) if (condition) [[unlikely]]
#elif defined(__GNUC__)
#define QKA_SQD_IF_UNLIKELY_(condition) if (__builtin_expect(!!(condition), 0))
#else
#define QKA_SQD_IF_UNLIKELY_(condition) if (condition)
#endif

namespace Qiskit
{

namespace addon
{

namespace sqd
{

/// Functor which returns `true` if a bitstring has a predetermined Hamming weight.
template <typename UnsignedType = uint32_t>
class MatchesRightLeftHamming
{
    UnsignedType right_target, left_target;

  public:
    // NOLINTBEGIN(bugprone-easily-swappable-parameters)
    /// Constructor.
    explicit MatchesRightLeftHamming(
        UnsignedType right_target, UnsignedType left_target
    )
      : right_target(right_target), left_target(left_target)
    {
    }
    // NOLINTEND(bugprone-easily-swappable-parameters)

    /// Function to be used as filter.
    ///
    /// @param[in] bitstring Bitstring to consider.
    ///
    /// @tparam BitstringType Type of `bitstring`, compatible with
    ///     `boost::dynamic_bitset<>` (for example).
    ///
    /// @return `true` if the bitstring matches the Hamming weight that the functor was
    ///     initialized with.
    template <typename BitstringType>
    bool operator()(const BitstringType &bitstring) const
    {
        QKA_SQD_IF_UNLIKELY_(bitstring.size() % 2 == 1)
        {
            QKA_SQD_THROW_INVALID_ARGUMENT_("`bitstring` must have even length");
        }
        const auto left_count = (bitstring >> (bitstring.size() / 2)).count();
        decltype(left_count) right_count = bitstring.count() - left_count;
        return right_count == right_target && left_count == left_target;
    }
};

/// Post-select bitstrings based on a given criteria.
///
/// @param[in] bitstrings Bitstrings to consider.
/// @param[in] weights Relative weight of each bitstring (need not be normalized to 1).
/// @param[in] filter_function Callable which returns a boolean indicating whether a
///     is to be kept.
///
/// @tparam BitstringVectorType Type of `bitstrings`, compatible with
///     `std::vector<boost::dynamic_bitset<>>`.
/// @tparam WeightVectorType Type of `weights`, compatible with `std::vector<double>`.
/// @tparam CallableType Type of `filter_function`, compatible with
///     `bool (*f)(const BitstringType &)`.
///
/// @return Post-selected bitstrings and their corresponding weights, normalized to 1.
///
/// # Example
///
///     std::vector<std::bitset<6>> bitstrings = {0b011010, 0b100011};
///     std::vector<double> weights = {0.1, 0.7};
///     auto [new_bitstrings, new_weights] = Qiskit::addon::sqd::postselect_bitstrings(
///         bitstrings, weights, Qiskit::addon::sqd::MatchesRightLeftHamming(1, 2)
///     );
template <
    typename BitstringVectorType, typename WeightVectorType, typename CallableType>
std::pair<BitstringVectorType, WeightVectorType> postselect_bitstrings(
    const BitstringVectorType &bitstrings, const WeightVectorType &weights,
    CallableType filter_function
)
{
    if (bitstrings.size() != weights.size()) {
        QKA_SQD_THROW_INVALID_ARGUMENT_(
            "`weights` must be same length as `bitstrings`"
        );
    }

    // Filter bitstrings
    BitstringVectorType filtered_bitstrings;
    WeightVectorType filtered_weights;
    auto current_bitstring = bitstrings.begin();
    auto current_weight = weights.begin();
    typename WeightVectorType::value_type filtered_weights_sum{};
    while (current_bitstring != bitstrings.end()) {
        if (filter_function(*current_bitstring)) {
            if (std::isnan(*current_weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("NaN found in weight array");
            }
            if (std::isinf(*current_weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Infinite value found in weight array");
            }
            if (*current_weight < 0) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Negative value found in weight array");
            }
            filtered_bitstrings.push_back(*current_bitstring);
            filtered_weights.push_back(*current_weight);
            filtered_weights_sum += *current_weight;
        }
        ++current_bitstring;
        ++current_weight;
    }

    // Normalize weights
    if (filtered_weights_sum != 0) {
        for (auto &weight : filtered_weights) {
            weight /= filtered_weights_sum;
        }
    }

    return {filtered_bitstrings, filtered_weights};
}

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_POSTSELECTION_HPP_
