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
// Confirms that both the current NoReplacementSampler and the Efraimidis-
// Spirakis EsSampler reproduce the exact sequential-scheme distribution.
//
// Method: for small (n, k), enumerate every ordered draw path to get the exact
// probability of each selected set.  Then, for each sampler, run many
// independent replicates and check that the log-likelihood-ratio (G) statistic
// follows chi-squared(dof): mean(G) ~ dof and P(G > crit95) ~ 5%.  A single G
// near the threshold is meaningless; the DISTRIBUTION of G over many seeds is
// the real test -- this is what gives confidence that E-S matches the target
// distribution.
//
// Build (from the repository root):
//   g++ -std=c++17 -O2 -I include \
//     -I deps/boost/dynamic_bitset/include -I deps/boost/config/include \
//     -I deps/boost/core/include -I deps/boost/assert/include \
//     -I deps/boost/integer/include -I deps/boost/static_assert/include \
//     -I deps/boost/throw_exception/include -I deps/boost/type_traits/include \
//     -I deps/boost/move/include \
//     docs/investigations/efraimidis-spirakis/verify_distribution.cpp -o verify

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>
#include <vector>

#include "qiskit/addon/sqd/internal/sample-without-replacement.hpp"

#include "es_sampler.hpp"

using Qiskit::addon::sqd::internal::NoReplacementSampler;
using Outcome = std::vector<std::size_t>;

static void enumerate_paths(
    const std::vector<double> &w, Outcome &chosen, double prob, double remaining_sum,
    std::size_t k, std::map<Outcome, double> &out
)
{
    if (chosen.size() == k) {
        Outcome key = chosen;
        std::sort(key.begin(), key.end());
        out[key] += prob;
        return;
    }
    for (std::size_t i = 0; i < w.size(); ++i) {
        if (w[i] <= 0.0) {
            continue;
        }
        if (std::find(chosen.begin(), chosen.end(), i) != chosen.end()) {
            continue;
        }
        chosen.push_back(i);
        enumerate_paths(w, chosen, prob * (w[i] / remaining_sum), remaining_sum - w[i], k, out);
        chosen.pop_back();
    }
}

static std::map<Outcome, double>
exact_distribution(const std::vector<double> &w, std::size_t k)
{
    double sum = 0.0;
    for (double x : w) {
        if (x > 0.0) {
            sum += x;
        }
    }
    std::map<Outcome, double> out;
    Outcome chosen;
    enumerate_paths(w, chosen, 1.0, sum, k, out);
    return out;
}

enum class Which { Current, ES };

template <Which W>
static double one_g(
    const std::vector<double> &w, std::size_t k,
    const std::map<Outcome, double> &exact, std::size_t trials, std::mt19937_64 &rng
)
{
    std::map<Outcome, std::uint64_t> counts;
    for (std::size_t t = 0; t < trials; ++t) {
        Outcome picked;
        if constexpr (W == Which::Current) {
            NoReplacementSampler<std::vector<double>> s(w);
            for (std::size_t i = 0; i < k; ++i) {
                picked.push_back(s(rng));
            }
        } else {
            EsSampler<std::vector<double>> s(w, rng);
            for (std::size_t i = 0; i < k; ++i) {
                picked.push_back(s(rng));
            }
        }
        std::sort(picked.begin(), picked.end());
        ++counts[picked];
    }
    double g = 0.0;
    for (const auto &[key, p] : exact) {
        if (p <= 0.0) {
            continue;
        }
        const double expected = p * trials;
        const auto it = counts.find(key);
        const double observed = (it == counts.end()) ? 0.0 : double(it->second);
        if (observed > 0.0) {
            g += 2.0 * observed * std::log(observed / expected);
        }
    }
    return g;
}

int main()
{
    struct Case {
        const char *name;
        std::vector<double> w;
        std::size_t k;
    };
    const std::vector<Case> cases = {
        {"ascending 1,2,3,4 k=2", {1, 2, 3, 4}, 2},
        {"1..6 k=5 (k near n)", {1, 2, 3, 4, 5, 6}, 5},
        {"dominant 5,1,1,1 k=2", {5, 1, 1, 1}, 2},
        {"zeros 0,2,3,0,5 k=2", {0, 2, 3, 0, 5}, 2},
    };

    const std::size_t runs = 400;
    const std::size_t trials = 200'000;
    std::mt19937_64 seed_seq(12345);

    std::printf("Distribution of G over %zu independent runs (%zu trials each).\n", runs, trials);
    std::printf("If a sampler matches the exact distribution: mean(G) ~ dof and P(G>crit95) ~ 5%%.\n\n");

    auto crit95 = [](int dof) {
        static const std::map<int, double> t = {{2, 5.991}, {5, 11.070}, {9, 16.919}};
        return t.at(dof);
    };

    for (const auto &c : cases) {
        const auto exact = exact_distribution(c.w, c.k);
        const int dof = int(exact.size()) - 1;
        for (const Which which : {Which::Current, Which::ES}) {
            double sum_g = 0.0;
            std::size_t exceed = 0;
            for (std::size_t r = 0; r < runs; ++r) {
                std::mt19937_64 rng(seed_seq());
                const double g = (which == Which::Current)
                                     ? one_g<Which::Current>(c.w, c.k, exact, trials, rng)
                                     : one_g<Which::ES>(c.w, c.k, exact, trials, rng);
                sum_g += g;
                if (g > crit95(dof)) {
                    ++exceed;
                }
            }
            std::printf(
                "  %-22s %-8s dof=%d  mean(G)=%.2f (expect ~%d)  exceed95=%.1f%%\n", c.name,
                which == Which::Current ? "current" : "E-S", dof, sum_g / runs, dof,
                100.0 * exceed / runs
            );
        }
    }
    return 0;
}
