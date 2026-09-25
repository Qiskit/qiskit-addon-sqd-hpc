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

#include <cassert>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

#include "qiskit/addon/sqd/internal/concepts.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"
#include "qiskit/addon/sqd/internal/finite-math.hpp"

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
    // replacement, under the sequential scheme: draw index i with probability
    // w_i / (sum of remaining weights), remove it, renormalize, and repeat.
    //
    // A Fenwick tree (binary indexed tree) over the weights supports both a
    // prefix-sum query and a point update in O(log n), and it can locate the
    // index for a target cumulative weight by descending the tree, also in
    // O(log n).  Each draw therefore costs O(log n) with no rejection and no
    // rebuilding: draw u in [0, total), find the index whose cumulative weight
    // interval contains u, then subtract that weight from the tree.
    using WeightType = typename WeightVectorType::value_type;

    // tree_[i] (1-based) holds the sum of weights over the range of leaves that
    // node i is responsible for.  tree_[0] is unused.
    std::vector<WeightType> tree_;
    // The current per-index weight, kept alongside the tree so that removing a
    // drawn index subtracts its exact original value.  Recovering the weight
    // from a difference of two Fenwick prefix sums would be subject to
    // floating-point cancellation, which could leave a tiny residual weight at a
    // supposedly-removed index (allowing it to be drawn again).
    std::vector<WeightType> weights_;
    WeightType total_weight_;
    std::size_t remaining_nonzero_weights;

    // Add `delta` to the weight at 0-based index `i`.
    void update(std::size_t i, WeightType delta)
    {
        for (std::size_t j = i + 1; j < tree_.size(); j += j & (~j + 1)) {
            tree_[j] += delta;
        }
    }

    // Return the smallest 0-based index j such that the cumulative weight
    // through j is strictly greater than `target` (0 <= target < total).  This
    // is the standard O(log n) Fenwick "find by cumulative frequency" descent.
    //
    // The descent itself is exact integer-index arithmetic over the node sums,
    // but those sums are accumulated floating-point values and so are subject to
    // cancellation: adding a small weight to a much larger one and later
    // subtracting the larger one back out does not restore the small one
    // (1e-12 + 1e12 - 1e12 == 0).  Once every remaining node sum has cancelled
    // to zero, `target` compares greater-or-equal at every step, the descent
    // takes all of them, and `pos` lands one past the last leaf.
    //
    // The comparison must stay untouched -- skipping a step without subtracting
    // that node's sum would corrupt the search and bias the distribution -- so
    // the correction happens here, after the descent: clamp into range, then
    // advance to a still-drawable leaf.  operator() has already established that
    // one exists.  This only engages once the weights involved have cancelled
    // away, i.e. when the sums can no longer express a meaningful preference
    // among what is left; the ordinary path is unaffected.
    std::size_t find_by_cumulative_weight(WeightType target) const
    {
        std::size_t pos = 0;
        // Largest power of two <= n.
        std::size_t step = 1;
        while (step << 1 < tree_.size()) {
            step <<= 1;
        }
        for (; step != 0; step >>= 1) {
            const std::size_t next = pos + step;
            if (next < tree_.size() && tree_[next] <= target) {
                pos = next;
                target -= tree_[next];
            }
        }
        if (pos >= weights_.size()) {
            pos = weights_.size() - 1;
        }
        if (weights_[pos] > WeightType{}) {
            return pos;
        }
        // `pos` landed on an already-drawn leaf, so the sums have cancelled and
        // can no longer express a preference among what is left.  Scan outward
        // for the nearest drawable leaf; operator() has established that one
        // exists, so this always finds it.
        for (std::size_t offset = 1; offset < weights_.size(); ++offset) {
            if (pos + offset < weights_.size() &&
                weights_[pos + offset] > WeightType{}) {
                return pos + offset;
            }
            if (pos >= offset && weights_[pos - offset] > WeightType{}) {
                return pos - offset;
            }
        }
        assert(false && "operator() guarantees a drawable leaf exists");
        return pos;
    }

  public:
    /// Constructor
    explicit NoReplacementSampler(const WeightVectorType &weights)
      : tree_(weights.size() + 1, WeightType{}), weights_(weights.size(), WeightType{}),
        total_weight_(WeightType{})
    {
        std::size_t nonzero_weights = 0;
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const auto weight = weights[i];
            // Check for any invalid argument
#if !QKA_SQD_FINITE_MATH_ONLY
            if (std::isnan(weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("NaN found in weight array");
            }
            if (std::isinf(weight)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Infinite value found in weight array");
            }
#endif // !QKA_SQD_FINITE_MATH_ONLY
            if (weight < 0) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Negative value found in weight array");
            }

            // Tally all nonzero weights
            if (weight > 0) {
                ++nonzero_weights;
                weights_[i] = weight;
                update(i, weight);
                total_weight_ += weight;
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
            // Also covers a sampler built entirely from zero weights, which never
            // had a drawable index to begin with.  Under -fno-exceptions this
            // message is the only diagnostic the caller gets (the macro prints
            // and terminates), so it names both situations.
            QKA_SQD_THROW_RUNTIME_ERROR_("No nonzero weights remain to draw from.");
        }
        --remaining_nonzero_weights;

        // Draw a target in [0, total_weight_) and locate the index whose
        // cumulative-weight interval contains it.
        //
        // total_weight_ is an accumulated sum and cancels like the tree nodes, so
        // once the large weights have been drawn it can reach zero or even go
        // negative (1e16 + 1 + 1 rounds to 1e16, so subtracting 1e16 and 1 back
        // out leaves -1).  uniform_real_distribution requires a <= b, so clamp
        // the upper bound; find_by_cumulative_weight then resolves the degenerate
        // target onto a drawable leaf.
        const WeightType upper =
            total_weight_ > WeightType{} ? total_weight_ : WeightType{};
        std::uniform_real_distribution<WeightType> dist(WeightType{}, upper);
        const std::size_t idx = find_by_cumulative_weight(dist(rng));

        // Remove the selected index so it cannot be drawn again, subtracting its
        // exact stored weight (see weights_).
        const WeightType selected_weight = weights_[idx];
        update(idx, -selected_weight);
        weights_[idx] = WeightType{};
        total_weight_ -= selected_weight;
        return idx;
    }
};

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_INTERNAL_SAMPLE_WITHOUT_REPLACEMENT_HPP_
