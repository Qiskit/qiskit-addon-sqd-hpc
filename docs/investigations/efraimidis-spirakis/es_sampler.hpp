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

// INVESTIGATION ARTIFACT -- not part of the shipped library.
//
// An Efraimidis-Spirakis (Algorithm A) implementation of weighted sampling
// without replacement, evaluated as a possible replacement for
// NoReplacementSampler.  Same sequential-scheme semantics, but exactly one RNG
// draw per candidate and no rejection/rebuild.  See README.md in this directory
// for the correctness and performance findings and the conclusion (not
// adopted).
//
// Key: for each candidate i with weight w_i, draw u_i ~ U(0,1] and form the key
// ln(u_i) / w_i (log domain, for numerical stability with extreme weights).
// Revealing candidates in descending key order reproduces the sequential
// proportional draw distribution (Efraimidis & Spirakis, 2006).
//
// Interface note: unlike NoReplacementSampler, this draws all randomness at
// construction (it needs the rng in the constructor); operator() then just
// reveals the next index.  That difference is deliberate -- drawing up front
// suits keyed counter-RNG substreams -- but it is an API change, which is one
// reason a swap is not free.

#ifndef QKA_SQD_INVESTIGATION_ES_SAMPLER_HPP_
#define QKA_SQD_INVESTIGATION_ES_SAMPLER_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "qiskit/addon/sqd/internal/concepts.hpp"
#include "qiskit/addon/sqd/internal/exception-macros.hpp"

template <typename WeightVectorType>
class EsSampler
{
    // (key, index) for each nonzero-weight candidate, sorted ascending so
    // operator() pops the largest key from the back.
    std::vector<std::pair<double, std::size_t>> keyed_;
    std::size_t next_ = 0;

  public:
    template <QKA_SQD_CONCEPT_RNG_(RNGType)>
    EsSampler(const WeightVectorType &weights, RNGType &rng)
    {
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        keyed_.reserve(weights.size());
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const auto w = weights[i];
#if !QKA_SQD_FINITE_MATH_ONLY
            if (std::isnan(w)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("NaN found in weight array");
            }
            if (std::isinf(w)) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Infinite value found in weight array");
            }
#endif
            if (w < 0) {
                QKA_SQD_THROW_INVALID_ARGUMENT_("Negative value found in weight array");
            }
            if (w > 0) {
                double u = unit(rng);
                if (u <= 0.0) {
                    u = std::numeric_limits<double>::min(); // keep ln(u) finite
                }
                keyed_.emplace_back(std::log(u) / w, i);
            }
        }
        std::sort(keyed_.begin(), keyed_.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
        next_ = keyed_.size();
    }

    std::size_t get_remaining_nonzero_weights() const { return next_; }

    // The rng is consumed at construction; operator() ignores it but keeps the
    // signature so it is otherwise a drop-in for NoReplacementSampler.
    template <QKA_SQD_CONCEPT_RNG_(RNGType)>
    std::size_t operator()(RNGType & /*rng*/)
    {
        if (next_ == 0) {
            QKA_SQD_THROW_RUNTIME_ERROR_(
                "Cannot draw more samples than number of nonzero weights."
            );
        }
        return keyed_[--next_].second;
    }
};

#endif // QKA_SQD_INVESTIGATION_ES_SAMPLER_HPP_
