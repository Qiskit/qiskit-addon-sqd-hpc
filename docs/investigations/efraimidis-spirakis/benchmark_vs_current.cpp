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
// Performance comparison of NoReplacementSampler (rejection + reused
// discrete_distribution) vs the Efraimidis-Spirakis EsSampler, over a range of
// (n, k) pairs including the ratios recover_configurations actually produces.
// Each measurement is the full cost of constructing a sampler and drawing k
// indices.
//
// Build (from the repository root):
//   g++ -std=c++17 -O3 -march=native -I include \
//     -I deps/nanobench/src/include \
//     -I deps/boost/dynamic_bitset/include -I deps/boost/config/include \
//     -I deps/boost/core/include -I deps/boost/assert/include \
//     -I deps/boost/integer/include -I deps/boost/static_assert/include \
//     -I deps/boost/throw_exception/include -I deps/boost/type_traits/include \
//     -I deps/boost/move/include \
//     docs/investigations/efraimidis-spirakis/benchmark_vs_current.cpp -o bench

#include <random>
#include <string>
#include <vector>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include "qiskit/addon/sqd/internal/sample-without-replacement.hpp"

#include "es_sampler.hpp"

using ankerl::nanobench::Bench;
using ankerl::nanobench::doNotOptimizeAway;
using Qiskit::addon::sqd::internal::NoReplacementSampler;

int main()
{
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> wdist(0.01, 1.0);

    struct Case {
        int n;
        int k;
        const char *note;
    };
    const std::vector<Case> cases = {
        {20, 10, "k/n=0.50"},   {100, 50, "k/n=0.50"},  {250, 150, "k/n=0.60"},
        {250, 20, "k/n=0.08"},  {500, 10, "k/n=0.02"},  {500, 250, "k/n=0.50"},
        {1000, 20, "k/n=0.02"}, {1000, 500, "k/n=0.50"}, {1000, 900, "k/n=0.90"},
    };

    for (const auto &c : cases) {
        std::vector<double> w;
        w.reserve(c.n);
        for (int i = 0; i < c.n; ++i) {
            w.push_back(wdist(rng));
        }
        Bench bench;
        bench.title(
            "n=" + std::to_string(c.n) + " k=" + std::to_string(c.k) + " (" + c.note + ")"
        );
        bench.relative(true).minEpochIterations(20000);
        bench.run("current", [&] {
            NoReplacementSampler<std::vector<double>> s(w);
            std::size_t acc = 0;
            for (int i = 0; i < c.k; ++i) {
                acc ^= s(rng);
            }
            doNotOptimizeAway(acc);
        });
        bench.run("E-S", [&] {
            EsSampler<std::vector<double>> s(w, rng);
            std::size_t acc = 0;
            for (int i = 0; i < c.k; ++i) {
                acc ^= s(rng);
            }
            doNotOptimizeAway(acc);
        });
    }
    return 0;
}
