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

#include <random>
#include <string>
#include <vector>

#include <nanobench.h>

#include "qiskit/addon/sqd/internal/sample-without-replacement.hpp"

using Qiskit::addon::sqd::internal::NoReplacementSampler;

// Measure the full cost of constructing a sampler over `n` weights and drawing
// `k` indices without replacement, across a range of pool sizes and draw
// fractions k/n.  This is the hot inner operation of configuration recovery.
static void benchmark_draw(ankerl::nanobench::Bench &bench, int n, int k)
{
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> wdist(0.01, 1.0);
    std::vector<double> weights;
    weights.reserve(n);
    for (int i = 0; i < n; ++i) {
        weights.push_back(wdist(rng));
    }

    bench.complexityN(k).run("n=" + std::to_string(n) + " k=" + std::to_string(k), [&] {
        NoReplacementSampler<std::vector<double>> sampler(weights);
        std::size_t acc = 0;
        for (int i = 0; i < k; ++i) {
            acc ^= sampler(rng);
        }
        ankerl::nanobench::doNotOptimizeAway(acc);
    });
}

void benchmark_sample_without_replacement(ankerl::nanobench::Bench &bench)
{
    bench.title("Sample without replacement (construct + draw k of n)");
    // Small pool through large, spanning small draw fractions (far from
    // half-filling) to large ones (near or above half-filling).
    benchmark_draw(bench, 20, 10);
    benchmark_draw(bench, 100, 50);
    benchmark_draw(bench, 250, 20);
    benchmark_draw(bench, 250, 150);
    benchmark_draw(bench, 500, 10);
    benchmark_draw(bench, 500, 250);
    benchmark_draw(bench, 1000, 20);
    benchmark_draw(bench, 1000, 500);
    benchmark_draw(bench, 1000, 900);
}
