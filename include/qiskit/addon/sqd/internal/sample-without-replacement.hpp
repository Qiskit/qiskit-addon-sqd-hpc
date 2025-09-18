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

#ifndef QISKIT_ADDON_SQD_INTERNAL_SAMPLE_WITHOUT_REPLACEMENT_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_SAMPLE_WITHOUT_REPLACEMENT_HPP_

#include <cstddef>
#include <random>
#include <vector>

#include "qiskit/addon/sqd/internal/concepts.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

/// Utility class for sampling without replacement.
template <typename WeightVectorType>
class NoReplacementSampler
{
  private:
    // Sample from the indices corresponding to the `weights`, without
    // replacement.  In order to do so, we will make a copy of the weights
    // vector.  Each time a sample is drawn, we change the corresponding weight
    // to zero.  However, we continue to use the same
    // std::discrete_distribution in order to avoid paying the O(N) cost
    // required to create the std::discrete_distribution for each sample.
    // However, any time we *do* draw indices that have been drawn before
    // on `num_retries` consecutive tries, we re-create the distribution, under
    // the assumption that it has grown too dense with indices that have
    // been sampled already.
    static constexpr int num_retries = 2;
    std::vector<typename WeightVectorType::value_type> working_weights;
    std::discrete_distribution<> dist;
    std::size_t remaining_nonzero_weights;

  public:
    /// Constructor
    explicit NoReplacementSampler(const WeightVectorType &weights)
      : working_weights(weights), dist(working_weights.begin(), working_weights.end())
    {
        std::size_t nonzero_weights = 0;
        for (auto weight : weights) {
            // Check for any invalid argument
            if (std::isnan(weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("NaN found in weight array");
            }
            if (std::isinf(weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Infinite value found in weight array");
            }
            if (weight < 0) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Negative value found in weight array");
            }

            // Tally all nonzero weights
            if (weight > 0) {
                ++nonzero_weights;
            }
        }
        remaining_nonzero_weights = nonzero_weights;
    }

    // Delete copy constructor and assignment operator
    NoReplacementSampler(const NoReplacementSampler &) = delete;
    NoReplacementSampler &operator=(const NoReplacementSampler &) = delete;

    /// Return the number of remaining samples that can be drawn
    std::size_t get_remaining_nonzero_weights() const
    {
        return remaining_nonzero_weights;
    }

    /// Sample a single value
    template <QKA_SQD_CONCEPT_RNG_(RNGType)>
    std::size_t operator()(RNGType &rng)
    {
        if (remaining_nonzero_weights == 0) {
            QKA_SQD_THROW_RUNTIME_ERROR_(
                "Cannot draw more samples than number of nonzero weights."
            );
        }
        --remaining_nonzero_weights;

        for (;;) {
            auto remaining_retries = num_retries;
            // Draw up to `num_retries` samples to find one with nonzero
            // `working_weight`
            do {
                const auto idx = dist(rng);
                if (working_weights[idx] != 0) {
                    // We found a sample that has not been sampled yet.  Select it, and
                    // mark it as ineligible for selection again.
                    working_weights[idx] = 0;
                    return idx;
                }
                --remaining_retries;
            } while (remaining_retries != 0);

            // We performed the loop `num_retries` times, but obtained only
            // samples that we had drawn previously.  So we reconstruct the
            // distribution in order to draw more samples without replacement.
            dist.param({working_weights.begin(), working_weights.end()});
        }
    }
};

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_INTERNAL_SAMPLE_WITHOUT_REPLACEMENT_HPP_
