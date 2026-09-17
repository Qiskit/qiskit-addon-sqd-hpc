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

// Distributional tests for NoReplacementSampler.
//
// The sampler implements weighted sampling WITHOUT replacement under the
// sequential ("successive") scheme: draw index i with probability
// w_i / sum(remaining weights), remove it, renormalize, repeat.  This is the
// same scheme exposed by NumPy's Generator.choice(replace=False, p=...), Julia
// StatsBase, and R's sample().  It is NOT inclusion-probability-proportional-
// to-size: the marginal probability that i appears in the final set is not
// proportional to w_i, and testing against that (wrong) invariant is a classic
// way to conclude a correct sampler is broken.
//
// For small (n, k) the exact distribution over selected sets is computed by
// enumerating every ordered draw path (a product of renormalized weights), and
// the sampler's empirical distribution is compared against it with a
// log-likelihood-ratio (G) test.  The RNG is fixed-seeded so the test is
// deterministic; the pass threshold is deliberately loose (well into the tail
// of the chi-squared null) so that a correct sampler does not flake, while a
// grossly wrong distribution still fails.

#include "doctest.h"

#include "qiskit/addon/sqd/internal/sample-without-replacement.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <vector>

using Qiskit::addon::sqd::internal::NoReplacementSampler;

using Outcome = std::vector<std::size_t>; // sorted selected indices

// Accumulate, over all ordered draw paths, the probability of each sorted
// selected set under the sequential scheme.
static void enumerate_paths(
    const std::vector<double> &weights, Outcome &chosen, double prob,
    double remaining_sum, std::size_t k, std::map<Outcome, double> &out
)
{
    if (chosen.size() == k) {
        Outcome key = chosen;
        std::sort(key.begin(), key.end());
        out[key] += prob;
        return;
    }
    for (std::size_t i = 0; i < weights.size(); ++i) {
        if (weights[i] <= 0.0) {
            continue;
        }
        if (std::find(chosen.begin(), chosen.end(), i) != chosen.end()) {
            continue;
        }
        chosen.push_back(i);
        enumerate_paths(
            weights, chosen, prob * (weights[i] / remaining_sum),
            remaining_sum - weights[i], k, out
        );
        chosen.pop_back();
    }
}

static std::map<Outcome, double>
exact_distribution(const std::vector<double> &weights, std::size_t k)
{
    double sum = 0.0;
    for (double w : weights) {
        if (w > 0.0) {
            sum += w;
        }
    }
    std::map<Outcome, double> out;
    Outcome chosen;
    enumerate_paths(weights, chosen, 1.0, sum, k, out);
    return out;
}

// Draw k indices per trial and tally the sorted selected sets.
template <typename RNGType>
static std::map<Outcome, std::uint64_t> empirical_counts(
    const std::vector<double> &weights,
    std::size_t k, // NOLINT(bugprone-easily-swappable-parameters)
    std::size_t trials, RNGType &rng
)
{
    std::map<Outcome, std::uint64_t> counts;
    for (std::size_t t = 0; t < trials; ++t) {
        NoReplacementSampler<std::vector<double>> sampler(weights);
        Outcome picked;
        for (std::size_t i = 0; i < k; ++i) {
            picked.push_back(sampler(rng));
        }
        std::sort(picked.begin(), picked.end());
        ++counts[picked];
    }
    return counts;
}

// G = 2 * sum O * ln(O / E).  Asymptotically chi-squared with
// (#outcomes - 1) degrees of freedom under the null that empirical == exact.
static double g_statistic(
    const std::map<Outcome, double> &exact,
    const std::map<Outcome, std::uint64_t> &counts, std::size_t trials
)
{
    double g = 0.0;
    for (const auto &[outcome, p] : exact) {
        if (p <= 0.0) {
            continue;
        }
        const double expected = p * static_cast<double>(trials);
        const auto it = counts.find(outcome);
        const double observed =
            (it == counts.end()) ? 0.0 : static_cast<double>(it->second);
        if (observed > 0.0) {
            g += 2.0 * observed * std::log(observed / expected);
        }
    }
    return g;
}

// The empirical support must not contain any outcome the exact distribution
// assigns zero probability (e.g. selecting a zero-weight index).
static bool support_is_subset(
    const std::map<Outcome, double> &exact,
    const std::map<Outcome, std::uint64_t> &counts
)
{
    for (const auto &[outcome, count] : counts) {
        if (count == 0) {
            continue;
        }
        const auto it = exact.find(outcome);
        if (it == exact.end() || it->second <= 0.0) {
            return false;
        }
    }
    return true;
}

TEST_CASE("NoReplacementSampler matches the exact sequential-scheme distribution")
{
    struct Case {
        const char *name;
        std::vector<double> weights;
        std::size_t k;
        // Loose per-case bound: comfortably beyond the chi-squared tail for the
        // relevant degrees of freedom, so a correct sampler passes reliably.
        double g_bound;
    };

    // dof = (#outcomes - 1).  For reference, chi-squared 99.9% critical values:
    // dof 2 -> 13.8, dof 5 -> 20.5, dof 9 -> 27.9.  Bounds below sit above those.
    const std::vector<Case> cases = {
        {"ascending weights", {1, 2, 3, 4}, 2, 25.0},
        {"five weights, k=3", {1, 2, 3, 4, 5}, 3, 35.0},
        {"one dominant weight", {5, 1, 1, 1}, 2, 25.0},
        {"uniform weights", {1, 1, 1, 1}, 2, 25.0},
        {"zero weights mixed in", {0, 2, 3, 0, 5}, 2, 20.0},
        {"k near n", {1, 2, 3, 4, 5, 6}, 5, 25.0},
        {"extreme weight spread", {1e-6, 1.0, 1e6}, 2, 20.0},
    };

    // Fixed seed -> deterministic test (a regression check on the distribution),
    // no run-to-run flakiness.
    std::mt19937_64 rng(0x5121'1f53'd251'1f53ULL);
    constexpr std::size_t trials = 500'000;

    for (const auto &c : cases) {
        CAPTURE(c.name);
        const auto exact = exact_distribution(c.weights, c.k);
        const auto counts = empirical_counts(c.weights, c.k, trials, rng);

        CHECK(support_is_subset(exact, counts));
        const double g = g_statistic(exact, counts, trials);
        CHECK(g < c.g_bound);
    }
}

TEST_CASE("NoReplacementSampler never selects a zero-weight index")
{
    const std::vector<double> weights{0.0, 1.0, 0.0, 2.0, 0.0};
    std::mt19937_64 rng(1234);
    for (int trial = 0; trial < 10'000; ++trial) {
        NoReplacementSampler<std::vector<double>> sampler(weights);
        for (int i = 0; i < 2; ++i) { // two nonzero weights available
            const std::size_t idx = sampler(rng);
            CHECK(weights[idx] > 0.0);
        }
    }
}
